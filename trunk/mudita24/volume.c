/*
 * volume.c - analog volume settings
 *
 * This code is added by Takashi Iwai <tiwai@suse.de>
 *
 * Copyright (c) 2000 Jaroslav Kysela <perex@perex.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <math.h>
#include "envy24control.h"

#define toggle_set(widget, state) \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state);

#define DAC_VOLUME_NAME	"DAC Volume"
#define ADC_VOLUME_NAME	"ADC Volume"
#define IPGA_VOLUME_NAME "IPGA Analog Capture Volume"
#define DAC_SENSE_NAME	"Output Sensitivity Switch"
#define ADC_SENSE_NAME	"Input Sensitivity Switch"

static int dac_volumes;
static int dac_max = 127;
static int adc_max = 127;
static int adc_volumes;
static int ipga_volumes;
static int dac_senses;
static int adc_senses;
static int dac_sense_items;
static int adc_sense_items;
static char *dac_sense_name[4];
static char *adc_sense_name[4];
extern int input_channels, output_channels;

int envy_dac_volumes(void)
{
	return dac_volumes;
}

int envy_dac_max(void)
{
	return dac_max;
}

int envy_adc_volumes(void)
{
	return adc_volumes;
}

int envy_adc_max(void)
{
	return adc_max;
}

int envy_ipga_volumes(void)
{
	return ipga_volumes;
}

int envy_dac_senses(void)
{
	return dac_senses;
}

int envy_adc_senses(void)
{
	return adc_senses;
}

int envy_dac_sense_items(void)
{
	return dac_sense_items;
}

int envy_adc_sense_items(void)
{
	return adc_sense_items;
}

const char *envy_dac_sense_enum_name(int i)
{
	return dac_sense_name[i];
}

const char *envy_adc_sense_enum_name(int i)
{
	return adc_sense_name[i];
}

int envy_analog_volume_available(void)
{
	return dac_volumes > 0 || adc_volumes > 0 || ipga_volumes > 0;
}


/*
 */

void dac_volume_update(int idx)
{
	snd_ctl_elem_value_t *val;
	int err;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, DAC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read dac volume: %s\n", snd_strerror(err));
		return;
	}
	gtk_adjustment_set_value(GTK_ADJUSTMENT(av_dac_volume_adj[idx]),
				 -snd_ctl_elem_value_get_integer(val, 0));
}

void adc_volume_update(int idx)
{
	snd_ctl_elem_value_t *val;
	int err;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, ADC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read adc volume: %s\n", snd_strerror(err));
		return;
	}
	gtk_adjustment_set_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx]),
				 -snd_ctl_elem_value_get_integer(val, 0));
	snd_ctl_elem_value_set_name(val, IPGA_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read ipga volume: %s\n", snd_strerror(err));
		return;
	}
	if (ipga_volumes > 0)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx]),
					 -0);
}

void ipga_volume_update(int idx)
{
	snd_ctl_elem_value_t *val;
	int err, ipga_vol;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, IPGA_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read ipga volume: %s\n", snd_strerror(err));
		return;
	}
	gtk_adjustment_set_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx]),
				 -(ipga_vol = snd_ctl_elem_value_get_integer(val, 0)));
	snd_ctl_elem_value_set_name(val, ADC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read adc volume: %s\n", snd_strerror(err));
		return;
	}
	// set ADC volume to max if IPGA volume greater 0
	if (ipga_vol)
		gtk_adjustment_set_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx]),
					 -adc_max);
}

void dac_sense_update(int idx)
{
	snd_ctl_elem_value_t *val;
	int err;
	int state;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, DAC_SENSE_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read dac sense: %s\n", snd_strerror(err));
		return;
	}
	state = snd_ctl_elem_value_get_enumerated(val, 0);
	toggle_set(av_dac_sense_radio[idx][state], TRUE);
}

void adc_sense_update(int idx)
{
	snd_ctl_elem_value_t *val;
	int err;
	int state;
	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, ADC_SENSE_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read adc sense: %s\n", snd_strerror(err));
		return;
	}
	state = snd_ctl_elem_value_get_enumerated(val, 0);
	toggle_set(av_adc_sense_radio[idx][state], TRUE);
}

/*
 */

static char temp_label[16];

/*
 * NPM: Per Fons Adriaensen''s message to linux-audio-user's list
 * July 2010 ( http://www.linuxaudio.org/mailarchive/lad/2010/7/13/171540 )
 * "L dB (L < 0) corresponds to 255 * (10 ^ L / 20)"
 * Alternately: http://en.wikipedia.org/wiki/Decibel#Field_quantities
 * 20 log10(V1/V0) where V1 is voltage measured and V0 is reference.
 * ... of course, the value of the digital peak meters isn't the "voltage"
 * for example an output of 1/255 --> −48.130803609dB
 * whereas 255/255 --> 0dB
 *         254/255 --> −0.034129276
 *         250/255 --> −0.172003435
 * http://alsa.cybermirror.org/manuals/icensemble/envy24.pdf states 
 * "Peak data derived from the absolute value of 9 msb.
 *  00h min - FFh max volume. Reading the register
 *  resets the meter to 00h."
 */
char* peak_level_to_db(int ival) {
  if (ival != 0) {
    double value = 20.0 * log10((double)ival/(double)MAX_METERING_LEVEL);
    //"(Off)"
    //"0dBFS" <-- seems to cause a resize oscillation, use 0.0dB instead
    //"0.0dB"
    //"-0.10"      
    //"-9.90"
    //"-48.0"
    if (value == 0.0)
      sprintf(temp_label, "0.0dB");
    else if (value > -10.0)
      sprintf(temp_label, "%+1.2f", value);
    else
      sprintf(temp_label, "%+2.1f", value);
    return (temp_label);
  }
  else
    return ("(Off)");
}

/*
 * NPM: "subroutine" used twice in create_mixer_frame() to draw markings
 * for each digital mixer input attenuation slider.  Each slider is drawn
 * optionally with or without a legend describing the dBs attenuation at
 * the given level.  Each slider controls attentuation of its input from
 * +0dB to -144.0dB (and "Off").
 */
void draw_24bit_attenuator_scale_markings(GtkScale *scale, GtkPositionType position, int draw_legend_p) {
  if (!no_scale_marks) {
    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE,
		       position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>": NULL);
    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+1*MIXER_ATTENUATOR_STEP_SIZE,
		       position, draw_legend_p ? "<span color='blue' size='x-small'>-12</span>" : NULL);
    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+2*MIXER_ATTENUATOR_STEP_SIZE,
		       position, draw_legend_p ? "<span color='blue' size='x-small'>-24</span>" : NULL);
    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+3*MIXER_ATTENUATOR_STEP_SIZE,
		       position, draw_legend_p ? "<span color='blue' size='x-small'>-36</span>" : NULL);
    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+4*MIXER_ATTENUATOR_STEP_SIZE,
		       position, draw_legend_p ? "<span color='blue' size='x-small'>-48</span>" : NULL);
    gtk_scale_add_mark(scale, (float) LOW_MIXER_ATTENUATION_VALUE,
		       position, NULL); /* NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) */

//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE,
//		       position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>": NULL);
//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+1*MIXER_ATTENUATOR_STEP_SIZE,
//		       position, draw_legend_p ? "<span color='blue' size='x-small'>-18</span>" : NULL);
//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+2*MIXER_ATTENUATOR_STEP_SIZE,
//		       position, draw_legend_p ? "<span color='blue' size='x-small'>-36</span>" : NULL);
//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+3*MIXER_ATTENUATOR_STEP_SIZE,
//		       position, draw_legend_p ? "<span color='blue' size='x-small'>-54</span>" : NULL);
//    //  gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+4*MIXER_ATTENUATOR_STEP_SIZE,
//    //		     position, draw_legend_p ? "<span color='blue' size='x-small'>-72</span>" : NULL);
//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+5*MIXER_ATTENUATOR_STEP_SIZE,
//		       position, draw_legend_p ? "<span color='blue' size='x-small'>-90</span>" : NULL);
//    //  gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+6*MIXER_ATTENUATOR_STEP_SIZE,
//    //		     position, draw_legend_p ? "<span color='blue' size='x-small'>-108</span>" : NULL);
//    gtk_scale_add_mark(scale, (float) MIN_MIXER_ATTENUATION_VALUE+7*MIXER_ATTENUATOR_STEP_SIZE,
//		       position, draw_legend_p ? "<span color='blue' size='x-small'>-126</span>" : NULL);
//    gtk_scale_add_mark(scale, (float) MAX_MIXER_ATTENUATION_VALUE,
//		       position, NULL); /* NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) */
  }
}

/* NPM: used in create_analog_volume() to draw dB markings on DAC attenuators */
void draw_dac_scale_markings(GtkScale *scale, GtkPositionType position) {
  if (!no_scale_marks) {
  gtk_scale_add_mark(scale, (float) -envy_dac_max(),
		     position, "<span color='green' size='x-small'>+0</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+1*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-6</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+2*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-12</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+3*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-18</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+4*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-24</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+5*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-30</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+6*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-36</span>");
//  gtk_scale_add_mark(scale, (float) -envy_dac_max()+7*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-42</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+8*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-48</span>");
//  gtk_scale_add_mark(scale, (float) -envy_dac_max()+9*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-54</span>");
  gtk_scale_add_mark(scale, (float) -envy_dac_max()+10*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-60</span>");
  gtk_scale_add_mark(scale, (float) 0,
		     position, NULL); /* NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) */
  }
}

/* NPM: used in create_analog_volume() to draw dB markings on ADC attenuators/amplifiers */
void draw_adc_scale_markings(GtkScale *scale, GtkPositionType position) {
  if (!no_scale_marks) {
  gtk_scale_add_mark(scale, (float) -envy_adc_max(),
		     position, "<span color='red' size='x-small'>+18</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+1*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='red' size='x-small'>+12</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+2*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='red' size='x-small'>+6</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+3*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='green' size='x-small'>+0</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+4*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-6</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+5*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-12</span>");
//  gtk_scale_add_mark(scale, (float) -envy_adc_max()+6*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-18</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+7*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-24</span>");
//  gtk_scale_add_mark(scale, (float) -envy_adc_max()+8*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-30</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+9*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-36</span>");
//  gtk_scale_add_mark(scale, (float) -envy_adc_max()+10*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-42</span>");
  gtk_scale_add_mark(scale, (float) -envy_adc_max()+11*ANALOG_GAIN_STEP_SIZE,
		     position, "<span color='blue' size='x-small'>-48</span>");
//  gtk_scale_add_mark(scale, (float) -envy_adc_max()+12*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-54</span>");
//  gtk_scale_add_mark(scale, (float) -envy_adc_max()+13*ANALOG_GAIN_STEP_SIZE,
//		     position, "<span color='blue' size='x-small'>-60</span>");
  gtk_scale_add_mark(scale, (float) 0,
		     position, NULL); /* NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) */
  }
}

void dac_volume_adjust(GtkAdjustment *adj, gpointer data)
{
	int idx = (int)(long)data;
	snd_ctl_elem_value_t *val;
	int err, ival = -(int)adj->value;

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, DAC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	snd_ctl_elem_value_set_integer(val, 0, ival);

	if ((err = snd_ctl_elem_write(ctl, val)) < 0) {
	  g_print("Unable to write dac volume: %s\n", snd_strerror(err));
	  sprintf(temp_label, "(Err)");
	}
	else if (ival == 0) {
	  sprintf(temp_label, "(Off)");
	}
	else {
	  /* NPM: changed to output dB values. Use of proper ALSA API
	     snd_ctl_convert_to_dB() to return dB values suggested by
	     Tim E. Real on linux-audio-devel list. */
	  snd_ctl_elem_id_t *elem_id;
	  snd_ctl_elem_id_alloca(&elem_id);
	  snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
	  snd_ctl_elem_id_set_name(elem_id, DAC_VOLUME_NAME);
	  snd_ctl_elem_id_set_index(elem_id, idx); // TER: Added
	  long db_gain = 0;
	  snd_ctl_convert_to_dB(ctl, elem_id, ival, &db_gain); /* convert ival integer to dB */
	  float fval = ((float)db_gain / 100.0);
	  if (fval <= -10)
	    sprintf(temp_label, "%+2.1f", fval);
	  else
	    sprintf(temp_label, "%+2.1f ", fval);
	}

	gtk_label_set_text(GTK_LABEL(av_dac_volume_label[idx]), temp_label);
}

void adc_volume_adjust(GtkAdjustment *adj, gpointer data)
{
	int idx = (int)(long)data;
	snd_ctl_elem_value_t *val;
	int err, ival = -(int)adj->value;

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, ADC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	snd_ctl_elem_value_set_integer(val, 0, ival);

	if ((err = snd_ctl_elem_write(ctl, val)) < 0) {
	  g_print("Unable to write adc volume: %s\n", snd_strerror(err));
	  sprintf(temp_label, "(Err)");
	}
	else if (ival == 0) {
	  sprintf(temp_label, "(Off)");
	}
	else {
	/* NPM: changed to output dB values. Use of proper ALSA API
	   snd_ctl_convert_to_dB() to return dB values suggested by
	   Tim E. Real on linux-audio-devel list. */
	  snd_ctl_elem_id_t *elem_id;
	  snd_ctl_elem_id_alloca(&elem_id);
	  snd_ctl_elem_id_clear(elem_id);
	  snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
	  snd_ctl_elem_id_set_name(elem_id, ADC_VOLUME_NAME);
	  long db_gain = 0;
	  snd_ctl_convert_to_dB(ctl, elem_id, ival, &db_gain);
	  float fval = ((float)db_gain / 100.0);
	  if (fval >= 10)
	    sprintf(temp_label, "%+2.1f", fval);
	  else if (fval > 0)
	    sprintf(temp_label, "%+2.1f ", fval);
	  else if (fval <= -10)
	    sprintf(temp_label, "%+2.1f", fval);
	  else
	    sprintf(temp_label, "%+2.1f ", fval);
	}

	gtk_label_set_text(GTK_LABEL(av_adc_volume_label[idx]), temp_label);
}

void ipga_volume_adjust(GtkAdjustment *adj, gpointer data)
{
	int idx = (int)(long)data;
	snd_ctl_elem_value_t *val;
	int err, ival = -(int)adj->value;
	char text[16];

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, IPGA_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	snd_ctl_elem_value_set_integer(val, 0, ival);
	sprintf(text, "%03i", ival);
	gtk_label_set_text(GTK_LABEL(av_ipga_volume_label[idx]), text);
	if ((err = snd_ctl_elem_write(ctl, val)) < 0)
		g_print("Unable to write ipga volume: %s\n", snd_strerror(err));
}

void dac_sense_toggled(GtkWidget *togglebutton, gpointer data)
{
	int idx = (long)data >> 8;
	int state = (long)data & 0xff;
	snd_ctl_elem_value_t *val;
	int err;

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, DAC_SENSE_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	snd_ctl_elem_value_set_enumerated(val, 0, state);
	if ((err = snd_ctl_elem_write(ctl, val)) < 0)
		g_print("Unable to write dac sense: %s\n", snd_strerror(err));
}

void adc_sense_toggled(GtkWidget *togglebutton, gpointer data)
{
	int idx = (long)data >> 8;
	int state = (long)data & 0xff;
	snd_ctl_elem_value_t *val;
	int err;

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(val, ADC_SENSE_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	snd_ctl_elem_value_set_enumerated(val, 0, state);
	if ((err = snd_ctl_elem_write(ctl, val)) < 0)
		g_print("Unable to write adc sense: %s\n", snd_strerror(err));
}

/*
 */

void analog_volume_init(void)
{
	snd_ctl_elem_info_t *info;
	int i;

	snd_ctl_elem_info_alloca(&info);

	snd_ctl_elem_info_set_interface(info, SND_CTL_ELEM_IFACE_MIXER);
	for (i = 0; i < 10; i++) {
		snd_ctl_elem_info_set_name(info, DAC_VOLUME_NAME);
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, i);
		if (snd_ctl_elem_info(ctl, info) < 0)
			break;
		dac_max = snd_ctl_elem_info_get_max(info);
	}
	if (i < output_channels - 1)
		dac_volumes = i;
	else
		dac_volumes = output_channels;

	snd_ctl_elem_info_set_name(info, DAC_SENSE_NAME);
	for (i = 0; i < dac_volumes; i++) {
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, i);
		if (snd_ctl_elem_info(ctl, info) < 0)
			break;
	}
	dac_senses = i;
	if (dac_senses > 0) {
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, 0);
		snd_ctl_elem_info(ctl, info);
		dac_sense_items = snd_ctl_elem_info_get_items(info);
		for (i = 0; i < dac_sense_items; i++) {
			snd_ctl_elem_info_set_item(info, i);
			snd_ctl_elem_info(ctl, info);
			dac_sense_name[i] = strdup(snd_ctl_elem_info_get_item_name(info));
		}
	}

	for (i = 0; i < 10; i++) {
		snd_ctl_elem_info_set_name(info, ADC_VOLUME_NAME);
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, i);
		if (snd_ctl_elem_info(ctl, info) < 0)
			break;
		adc_max = snd_ctl_elem_info_get_max(info);
	}
	if (i < input_channels - 1)
		adc_volumes = i;
	else
		adc_volumes = input_channels;
	snd_ctl_elem_info_set_name(info, ADC_SENSE_NAME);
	for (i = 0; i < adc_volumes; i++) {
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, i);
		if (snd_ctl_elem_info(ctl, info) < 0)
			break;
	}
	adc_senses = i;
	if (adc_senses > 0) {
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, 0);
		snd_ctl_elem_info(ctl, info);
		adc_sense_items = snd_ctl_elem_info_get_items(info);
		for (i = 0; i < adc_sense_items; i++) {
			snd_ctl_elem_info_set_item(info, i);
			snd_ctl_elem_info(ctl, info);
			adc_sense_name[i] = strdup(snd_ctl_elem_info_get_item_name(info));
		}
	}

	for (i = 0; i < 10; i++) {
		snd_ctl_elem_info_set_name(info, IPGA_VOLUME_NAME);
		snd_ctl_elem_info_set_numid(info, 0);
		snd_ctl_elem_info_set_index(info, i);
		if (snd_ctl_elem_info(ctl, info) < 0)
			break;
	}
	if (i < input_channels - 1)
		ipga_volumes = i;
	else
		ipga_volumes = input_channels;
}

void analog_volume_postinit(void)
{
	int i;

	for (i = 0; i < dac_volumes; i++) {
		dac_volume_update(i);
		dac_volume_adjust((GtkAdjustment *)av_dac_volume_adj[i], (gpointer)(long)i);
	}
	for (i = 0; i < adc_volumes; i++) {
		adc_volume_update(i);
		adc_volume_adjust((GtkAdjustment *)av_adc_volume_adj[i], (gpointer)(long)i);
	}
	for (i = 0; i < ipga_volumes; i++) {
		ipga_volume_update(i);
		ipga_volume_adjust((GtkAdjustment *)av_ipga_volume_adj[i], (gpointer)(long)i);
	}
	for (i = 0; i < dac_senses; i++)
		dac_sense_update(i);
	for (i = 0; i < adc_senses; i++)
		adc_sense_update(i);
}
