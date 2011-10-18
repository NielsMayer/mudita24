/*****************************************************************************
   mixer.c - mixer code
   Copyright (C) 2000 by Jaroslav Kysela <perex@perex.cz>
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
******************************************************************************/

#include "envy24control.h"
#include "midi.h"
#include "config.h"

#define	MULTI_PLAYBACK_SWITCH		"Multi Playback Switch"
#define MULTI_PLAYBACK_VOLUME		"Multi Playback Volume"

#define HW_MULTI_CAPTURE_SWITCH		"H/W Multi Capture Switch"
#define IEC958_MULTI_CAPTURE_SWITCH	"IEC958 Multi Capture Switch"

#define HW_MULTI_CAPTURE_VOLUME		"H/W Multi Capture Volume"
#define IEC958_MULTI_CAPTURE_VOLUME	"IEC958 Multi Capture Volume"

#define toggle_set(widget, state) \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state);

static int stream_is_active[MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + \
				MAX_INPUT_CHANNELS + MAX_SPDIF_CHANNELS];
extern int input_channels, output_channels, pcm_output_channels, spdif_channels, view_spdif_playback;

static int is_active(GtkWidget *widget)
{
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? 1 : 0;
}

void mixer_update_stream(int stream, int vol_flag, int sw_flag)
{
	int err;
	
  //printf("mixer_update_stream stream:%d vol_flag:%d sw_flag:%d\n", stream, vol_flag, sw_flag);
  
  if (! stream_is_active[stream - 1])
		return;

	if (vol_flag) {
		snd_ctl_elem_value_t *vol;
		int v[2];
		snd_ctl_elem_value_alloca(&vol);
		snd_ctl_elem_value_set_interface(vol, SND_CTL_ELEM_IFACE_MIXER);
		snd_ctl_elem_value_set_name(vol, stream <= 10 ? MULTI_PLAYBACK_VOLUME : (stream <= 18 ? HW_MULTI_CAPTURE_VOLUME : IEC958_MULTI_CAPTURE_VOLUME));
		snd_ctl_elem_value_set_index(vol, stream <= 18 ? (stream - 1) % 10 : (stream - 1) % 18 );
		if ((err = snd_ctl_elem_read(ctl, vol)) < 0)
			g_print("Unable to read multi playback volume: %s\n", snd_strerror(err));
		v[0] = snd_ctl_elem_value_get_integer(vol, 0);
		v[1] = snd_ctl_elem_value_get_integer(vol, 1);
		if (v[0] != v[1])
			toggle_set(mixer_stereo_toggle[stream-1], FALSE);
    
		// TER: Stop jitter when adjusting sliders.
    //printf("mixer_update_stream cur val:%f new val:%d\n", GTK_ADJUSTMENT(mixer_adj[stream-1][0])->value, MAX_MIXER_ATTENUATION_VALUE - v[0]);
    if((gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(mixer_adj[stream-1][0])) != MAX_MIXER_ATTENUATION_VALUE - v[0])
      gtk_adjustment_set_value(GTK_ADJUSTMENT(mixer_adj[stream-1][0]), MAX_MIXER_ATTENUATION_VALUE - v[0]);
    if((gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(mixer_adj[stream-1][1])) != MAX_MIXER_ATTENUATION_VALUE - v[1])
		  gtk_adjustment_set_value(GTK_ADJUSTMENT(mixer_adj[stream-1][1]), MAX_MIXER_ATTENUATION_VALUE - v[1]);
      
		midi_controller((stream-1)*2,   v[0]);
		midi_controller((stream-1)*2+1, v[1]);
	}
	if (sw_flag) {
		snd_ctl_elem_value_t *sw;
		int v[2];
		snd_ctl_elem_value_alloca(&sw);
		snd_ctl_elem_value_set_interface(sw, SND_CTL_ELEM_IFACE_MIXER);
		snd_ctl_elem_value_set_name(sw, stream <= 10 ? MULTI_PLAYBACK_SWITCH : (stream <= 18 ? HW_MULTI_CAPTURE_SWITCH : IEC958_MULTI_CAPTURE_SWITCH));
		snd_ctl_elem_value_set_index(sw, stream <= 18 ? (stream - 1) % 10 : (stream - 1) % 18 );
		if ((err = snd_ctl_elem_read(ctl, sw)) < 0)
			g_print("Unable to read multi playback switch: %s\n", snd_strerror(err));
		v[0] = snd_ctl_elem_value_get_boolean(sw, 0);
		v[1] = snd_ctl_elem_value_get_boolean(sw, 1);
		if (v[0] != v[1])
			toggle_set(mixer_stereo_toggle[stream-1], FALSE);
		toggle_set(mixer_mute_toggle[stream-1][0], !v[0] ? TRUE : FALSE);
		toggle_set(mixer_mute_toggle[stream-1][1], !v[1] ? TRUE : FALSE);
		midi_button((stream-1)*2, v[0]);
		midi_button((stream-1)*2+1, v[1]);
	}
}

static void set_switch1(int stream, int left, int right)
{
	snd_ctl_elem_value_t *sw;
	int err, changed = 0;
	
	snd_ctl_elem_value_alloca(&sw);
	snd_ctl_elem_value_set_interface(sw, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(sw, stream <= 10 ? MULTI_PLAYBACK_SWITCH : (stream <= 18 ? HW_MULTI_CAPTURE_SWITCH : IEC958_MULTI_CAPTURE_SWITCH));
	snd_ctl_elem_value_set_index(sw, stream <= 18 ? (stream - 1) % 10 : (stream - 1) % 18 );
	if ((err = snd_ctl_elem_read(ctl, sw)) < 0)
		g_print("Unable to read multi switch: %s\n", snd_strerror(err));
	if (left >= 0 && left != snd_ctl_elem_value_get_boolean(sw, 0)) {
		snd_ctl_elem_value_set_boolean(sw, 0, left);
		changed = 1;
		midi_button((stream-1)*2, left);
	}
	if (right >= 0 && right != snd_ctl_elem_value_get_boolean(sw, 1)) {
		snd_ctl_elem_value_set_boolean(sw, 1, right);
		changed = 1;
		midi_button((stream-1)*2+1, right);
	}
	if (changed) {
		err = snd_ctl_elem_write(ctl, sw);
		if (err < 0)
			g_print("Unable to write multi switch: %s\n", snd_strerror(err));
	}
}

void mixer_toggled_mute(GtkWidget *togglebutton, gpointer data)
{
	int stream = (long)data >> 16;
	int button = (long)data & 1;
	int stereo = is_active(mixer_stereo_toggle[stream-1]) ? 1 : 0;
	int mute;
	int vol[2] = { -1, -1 };
	
	mute = is_active(mixer_mute_toggle[stream-1][button]);
	vol[button] = !mute;
	if (stereo) {
		toggle_set(mixer_mute_toggle[stream-1][button^1], mute);
		vol[button^1] = !mute;
	}
	set_switch1(stream, vol[0], vol[1]);
}

void mixer_set_mute(int stream, int left, int right)
{
	int stereo = is_active(mixer_stereo_toggle[stream-1]) ? 1 : 0;
	if (left >= 0 || stereo) {
		toggle_set(mixer_mute_toggle[stream-1][0], left ? TRUE : FALSE);
		if(stereo && left<0) left=right;
	}
	if (right >= 0 || stereo) {
		toggle_set(mixer_mute_toggle[stream-1][1], right ? TRUE : FALSE);
		if(stereo && right<0) right=left;
	}
	set_switch1(stream, left, right);
}

static void set_volume1(int stream, int left, int right)
{
	snd_ctl_elem_value_t *vol;
	int change = 0;
	int err;
	
	snd_ctl_elem_value_alloca(&vol);
	snd_ctl_elem_value_set_interface(vol, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(vol, stream <= 10 ? MULTI_PLAYBACK_VOLUME : (stream <= 18 ? HW_MULTI_CAPTURE_VOLUME : IEC958_MULTI_CAPTURE_VOLUME));
	snd_ctl_elem_value_set_index(vol, stream <= 18 ? (stream - 1) % 10 : (stream - 1) % 18 );
	if ((err = snd_ctl_elem_read(ctl, vol)) < 0)
		g_print("Unable to read multi volume: %s\n", snd_strerror(err));
	if (left >= 0) {
		change |= (snd_ctl_elem_value_get_integer(vol, 0) != left);
		snd_ctl_elem_value_set_integer(vol, 0, left);
		midi_controller((stream-1)*2, left);
	}
	if (right >= 0) {
		change |= (snd_ctl_elem_value_get_integer(vol, 1) != right);
		snd_ctl_elem_value_set_integer(vol, 1, right);
		midi_controller((stream-1)*2+1, right);
	}
	if (change) {
    if ((err = snd_ctl_elem_write(ctl, vol)) < 0 && err != -EBUSY)
			g_print("Unable to write multi volume: %s\n", snd_strerror(err));
	}
}

/* 
 * NPM: mixer_volume_to_db() -- called out of mixer_adjust(). Use of proper
 * ALSA API snd_ctl_convert_to_dB() to return dB values suggested by
 * Tim E. Real on linux-audio-devel list.
 */
static char temp_label[16];
static char* mixer_volume_to_db(int stream, int ival) {
  if (ival != 0) {
//  g_print("mixer_volume_to_db(%i, %i)\n", stream, ival);
    snd_ctl_elem_id_t *elem_id;
    snd_ctl_elem_id_alloca(&elem_id);
    snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
    /* NPM: IEC958_MULTI_CAPTURE_VOLUME, for stream=19 or 20 gives incorrect
     * results, use HW_MULTI_CAPTURE_VOLUME for all.
     * Verified by TER. Those two controls have no dB values, but they
     * should, they're just part of the same mixer ! */
    //snd_ctl_elem_id_set_name(elem_id, (stream <= 10) ? MULTI_PLAYBACK_VOLUME : (stream <= 18 ? HW_MULTI_CAPTURE_VOLUME : IEC958_MULTI_CAPTURE_VOLUME));
    snd_ctl_elem_id_set_name(elem_id, (stream <= 10) ? MULTI_PLAYBACK_VOLUME : HW_MULTI_CAPTURE_VOLUME);
    /* TER: First line is 'proper' way, but second line is corrected
     *  workaround for lack of dB values for IEC958 controls. */
    //snd_ctl_elem_id_set_index(elem_id, stream <= 18 ? (stream - 1) % 10 : (stream - 1) % 18 );
    snd_ctl_elem_id_set_index(elem_id, stream <= 18 ? (stream - 1) % 10 : 0 );
    long db_gain = 0;
    snd_ctl_convert_to_dB(ctl, elem_id, ival, &db_gain); /* convert 'ival' attenuation to mixer from integer to dB for display */
    float fval = ((float)db_gain / 100.0);
    if (fval > -10)
      sprintf(temp_label, "%+2.1f  ", fval);
    if (fval < -100)
      sprintf(temp_label, "%+2.1f", fval);
    else
      sprintf(temp_label, "%+2.1f ", fval);
  }
  else
    sprintf(temp_label,   "(Off) ");

  return (temp_label);
}


void mixer_adjust(GtkAdjustment *adj, gpointer data)
{
	int stream = (long)data >> 16;
	int button = (long)data & 1;
	int stereo = is_active(mixer_stereo_toggle[stream-1]) ? 1 : 0;
	int vol[2] = { -1, -1 };
	
  // TER: Stop jitter when adjusting sliders.
  int ival = (int)gtk_adjustment_get_value(adj);
  //printf("mixer_adjust stream:%d button:%d stereo:%d val:%f newval:%d ival:%d\n", 
  //       stream, button, stereo, gtk_adjustment_get_value(adj), (int)(MAX_MIXER_ATTENUATION_VALUE - gtk_adjustment_get_value(adj)), ival);
  
  //vol[button] = MAX_MIXER_ATTENUATION_VALUE - adj->value;
  vol[button] = MAX_MIXER_ATTENUATION_VALUE - ival;         // TER
  
	if (stereo) {
    // TER: Changed
    //gtk_adjustment_set_value(GTK_ADJUSTMENT(mixer_adj[stream-1][button ^ 1]), adj->value);
		//vol[button ^ 1] = MAX_MIXER_ATTENUATION_VALUE - adj->value;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(mixer_adj[stream-1][button ^ 1]), gtk_adjustment_get_value(adj)); 
    vol[button ^ 1] = MAX_MIXER_ATTENUATION_VALUE - ival;
	}
	if (vol[0] != -1) {
	  if (vol[0] <= (MAX_MIXER_ATTENUATION_VALUE - LOW_MIXER_ATTENUATION_VALUE))
	    vol[0] = MIN_MIXER_ATTENUATION_VALUE; /* reset to "off" if at bottom of shortened mixer scale */
	  gtk_label_set_text(GTK_LABEL(mixer_label[stream-1][0]),
			     (gchar*)mixer_volume_to_db(stream, vol[0]));
	}
	if (vol[1] != -1) {
	  if (vol[1] <= (MAX_MIXER_ATTENUATION_VALUE - LOW_MIXER_ATTENUATION_VALUE))
	    vol[1] = MIN_MIXER_ATTENUATION_VALUE; /* reset to "off" if at bottom of shortened mixer scale */
	  gtk_label_set_text(GTK_LABEL(mixer_label[stream-1][1]),
			     (gchar*)mixer_volume_to_db(stream, vol[1]));
	}
	set_volume1(stream, vol[0], vol[1]);
}

int mixer_stream_is_active(int stream)
{
	return stream_is_active[stream - 1];
}

void mixer_init(void)
{
	int i;
	int nb_active_channels;
	snd_ctl_elem_value_t *val;

	midi_maxstreams(sizeof(stream_is_active)/sizeof(stream_is_active[0]));

	snd_ctl_elem_value_alloca(&val);
	snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
	memset (stream_is_active, 0, (MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS + MAX_SPDIF_CHANNELS) * sizeof(int));
	snd_ctl_elem_value_set_name(val, MULTI_PLAYBACK_SWITCH);
	nb_active_channels = 0;
	for (i = 0; i < pcm_output_channels; i++) {
		snd_ctl_elem_value_set_numid(val, 0);
		snd_ctl_elem_value_set_index(val, i);
		if (snd_ctl_elem_read(ctl, val) < 0)
			continue;

		stream_is_active[i] = 1;
		nb_active_channels++;
	}
	pcm_output_channels = nb_active_channels;
	for (i = MAX_PCM_OUTPUT_CHANNELS; i < MAX_PCM_OUTPUT_CHANNELS + spdif_channels; i++) {
		snd_ctl_elem_value_set_numid(val, 0);
		snd_ctl_elem_value_set_index(val, i);
 		if (snd_ctl_elem_read(ctl, val) < 0)
			continue;
		stream_is_active[i] = 1;
	}
	snd_ctl_elem_value_set_name(val, HW_MULTI_CAPTURE_SWITCH);
	nb_active_channels = 0;
	for (i = 0; i < input_channels; i++) {
		snd_ctl_elem_value_set_numid(val, 0);
		snd_ctl_elem_value_set_index(val, i);
		if (snd_ctl_elem_read(ctl, val) < 0)
			continue;

		stream_is_active[i + MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS] = 1;
		nb_active_channels++;
	}
	input_channels = nb_active_channels;
	snd_ctl_elem_value_set_name(val, IEC958_MULTI_CAPTURE_SWITCH);
	for (i = 0; i < spdif_channels; i++) {
		snd_ctl_elem_value_set_numid(val, 0);
		snd_ctl_elem_value_set_index(val, i);
 		if (snd_ctl_elem_read(ctl, val) < 0)
			continue;
		stream_is_active[i + MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS] = 1;
	}
}

void mixer_postinit(void)
{
	int stream;

	for (stream = 1; stream <= pcm_output_channels; stream++) {
		if (stream_is_active[stream - 1])
			mixer_update_stream(stream, 1, 1);
	}
	for (stream = MAX_PCM_OUTPUT_CHANNELS + 1; \
		stream <= MAX_PCM_OUTPUT_CHANNELS + spdif_channels; stream++) {
		if (stream_is_active[stream - 1] && view_spdif_playback)
			mixer_update_stream(stream, 1, 1);
	}
	for (stream = MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + 1; \
		stream <= MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + input_channels; stream++) {
		if (stream_is_active[stream - 1])
			mixer_update_stream(stream, 1, 1);
	}
	for (stream = MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS + 1; \
		stream <= MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS + spdif_channels; stream++) {
		if (stream_is_active[stream - 1])
			mixer_update_stream(stream, 1, 1);
	}

	config_restore_stereo();
}

