/*****************************************************************************
   levelmeters.c - Stereo level meters
   Copyright (C) 2000 by Jaroslav Kysela <perex@perex.cz>
   Missing peak-level meter feature, and associated rewrite of metering for
   lower X resource usage, along with using a logarithmic display that attempts
   correspondence to mixer labels; updating text display of these levels in
   dBFS appears in their associated panels.
   Copyright (C) 2010 by Niels Mayer ( http://nielsmayer.com ).
   
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

#include <math.h>
#include "envy24control.h"

static GdkGC *penWhiteLight[21] = { NULL, };
static GdkGC *penGreenLight[21] = { NULL, };
static GdkGC *penForeground[21] = { NULL, };
static GdkGC *penBackground[21] = { NULL, };
static GdkGC *penOrangeLight[21] = { NULL, };
static GdkGC *penRedLight[21] = { NULL, };
static GdkColor *peak_label_color = NULL; /* NPM set in level_meters_configure_event() */

static GdkPixmap *pixmap[21] = { NULL, };
static snd_ctl_elem_value_t *peaks;

extern int input_channels, output_channels, pcm_output_channels, spdif_channels, view_spdif_playback;

static void update_peak_switch(void) {
	int err;

	if ((err = snd_ctl_elem_read(ctl, peaks)) < 0)
		g_print("Unable to read peaks: %s\n", snd_strerror(err));
}

/*
 * Niels Mayer (NPM) Jul-11-10: Fixing https://bugzilla.redhat.com/show_bug.cgi?id=602903
 * by implementing peak-level meters. The http://alsa.cybermirror.org/manuals/icensemble/envy24.pdf
 * soundchip provides hardware level meters and these are returned via ALSA as such:
 * >> amixer -c M66 cget iface=PCM,name='Multi Track Peak',numid=45
 *  ; type=INTEGER,access=r-------,values=22,min=0,max=255,step=0
 *  : values=0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,198,255,198 
 * These globals store the peak levels of the meters across callbacks.
 */
#define RESET -1
// NPM: Note special case in original envy24control code, which puts stereo mix track at
// "index 0" which is actually "(peaks, 20)" and "(peaks, 21)" cases below
// thus peak_lmix <--> peak_levels[IDX_LMIX] ; peak_rmix <--> peak_levels[IDX_RMIX]
static int peak_levels[MULTI_TRACK_PEAK_CHANNELS] = {0,};
static int peak_changed[MULTI_TRACK_PEAK_CHANNELS] = {0,};
static int previous_levels[MULTI_TRACK_PEAK_CHANNELS] = {0,};
/* NPM: changed for https://bugzilla.redhat.com/show_bug.cgi?id=602903 */
static void get_levels(int idx, int *l1, int *l2) {
  *l1 = *l2 = 0;
  if (idx == 0) { /* "if (stereo)" -- special case idx=0 as digital mix pair */
    if ((peak_changed[IDX_LMIX] != RESET) && (peak_changed[IDX_RMIX] != RESET)) { /* don't change values if doing "Reset Peaks" */
      if ((*l1 = snd_ctl_elem_value_get_integer(peaks, IDX_LMIX)) > peak_levels[IDX_LMIX]) {
	peak_levels[IDX_LMIX] = (*l1);
	peak_changed[IDX_LMIX] = TRUE;
      }
      if ((*l2 = snd_ctl_elem_value_get_integer(peaks, IDX_RMIX)) > peak_levels[IDX_RMIX]) {
	peak_levels[IDX_RMIX] = (*l2);
	peak_changed[IDX_RMIX] = TRUE;
      }
    }
  }
  else {
    if (peak_changed[idx-1] != RESET) {
      if ((*l1 = snd_ctl_elem_value_get_integer(peaks, idx - 1)) > peak_levels[idx - 1]) {
	peak_levels[idx - 1] = (*l1);
	peak_changed[idx - 1] = TRUE;
      }
    }
  }
}

static GdkGC *get_pen(int idx, int nRed, int nGreen, int nBlue) {
	GdkColor *c;
	GdkGC *gc;
	
	c = (GdkColor *)g_malloc(sizeof(GdkColor));
	c->red = nRed;
	c->green = nGreen;
	c->blue = nBlue;
	gdk_color_alloc(gdk_colormap_get_system(), c);
	gc = gdk_gc_new(pixmap[idx]);
	gdk_gc_set_foreground(gc, c);
	return gc;
}

static int get_index(const gchar *name) {
	int result;

	if (!strcmp(name, "DigitalMixer"))
		return 0;
	result = atoi(name + 5);
	if (result < 1 || result > 20) {
		g_print("Wrong drawing area ID: %s\n", name);
		gtk_main_quit();
	}
	return result;
}

/*
 * NPM: per http://alsa.cybermirror.org/manuals/icensemble/envy24.pdf the
 * the values in each entry of iface=PCM,name='Multi Track Peak',numid=45
 * represent: "Peak data derived from the absolute value of 9 msb. 00h
 * min - FFh max volume. Reading the register resets the meter to 00h."
 */

//NPM difft colors for -1dB, -3dB, and -6dB peak levels
#define GET_COLOR_FOR_PEAKLEVEL(level) \
  (level > 228)                        \
      ? (penRedLight[idx])             \
      : ((level > 181)                 \
	 ? (penOrangeLight[idx])       \
	 : ((level > 128)              \
	    ? (penWhiteLight[idx])     \
	    : (penGreenLight[idx])))   

/*
 * NPM: Called by redraw_meters(), this is a special case for when "Reset
 * Peaks" is clicked: then just refresh meters, ignore value, clear RESET
 * and let the next pass-through draw for the first time in the cleared
 * meter...
 */
static void draw_meters_reset(int idx, int width, int height,
			      int stereo, int segment_width) {
//  GdkColor color;
  GtkWidget* lbl;

//  if (!gdk_color_parse("bg", &color))
//    g_print("gdk_color_parse('bg') fail\n");

  gdk_draw_rectangle(pixmap[idx],
		     penBackground[idx], 
		     TRUE,
		     //X
		     6,
		     //Y
		     0,
		     //WIDTH
		     segment_width,
		     //HEIGHT
		     height
		     );
  /* NPM: Reset peak labels for "Monitor Inputs" and "Monitor PCMs" panels */
  gtk_label_set_text(GTK_LABEL((stereo) ? peak_label[IDX_LMIX] : peak_label[idx-1]),
		     peak_level_to_db((stereo) ? peak_levels[IDX_LMIX] : peak_levels[idx-1])); /* put new value in label */
  gtk_widget_modify_fg((stereo) ? peak_label[IDX_LMIX] : peak_label[idx-1],
		       GTK_STATE_NORMAL, NULL);

  /* NPM: Reset "Analog Volume" panel's peak levels -- never happens for "stereo" case */
  if (!stereo) {
    /* NPM: Reset peak labels for DACs in "Analog Volume" panel */
    if (((idx-1) >= 0) && ((idx-1) < envy_dac_volumes())) { /* index 0-8 corresponds to one of the eight DAC's */
      if (((idx-1) < MAX_OUTPUT_CHANNELS) /* make sure within bounds of dac_peak_label[] */
	  && (lbl = dac_peak_label[idx-1]) != NULL) {
	gtk_label_set_text(GTK_LABEL(lbl), peak_level_to_db(peak_levels[idx-1])); /* put new value in label */
	gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, NULL); /* reset fg color */
      }
    }
    /* NPM: Reset peak labels for ADCs in "Analog Volume" panel */
    else if (((idx-1) >= (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) /* ADC channels begin at 11 = MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS */
	     && ((idx-1) < (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS + envy_adc_volumes()))) { /* ADC channels end at 19 */
      if ((((idx-1) - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) < MAX_INPUT_CHANNELS) /* make sure within bounds of adc_peak_label[] */
	   && (lbl = adc_peak_label[(idx-1) - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)]) != NULL) {
	gtk_label_set_text(GTK_LABEL(lbl), peak_level_to_db(peak_levels[idx-1])); /* put new value in label */
	gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, NULL); /* reset fg color */
      }
    }
  }
  if (stereo) {
    gdk_draw_rectangle(pixmap[idx],
		       penBackground[idx], 
		       TRUE,
		       //X
		       2 + (width / 2),
		       //Y
		       0,
		       //WIDTH
		       segment_width,
		       //HEIGHT
		       height
		       );
    /* put new value in label */
    gtk_label_set_text(GTK_LABEL(peak_label[IDX_RMIX]), peak_level_to_db(peak_levels[IDX_RMIX])); 
    gtk_widget_modify_fg(peak_label[IDX_RMIX], GTK_STATE_NORMAL, NULL);

    peak_changed[IDX_LMIX] = FALSE;
    peak_changed[IDX_RMIX] = FALSE;
  }
  else {
    peak_changed[idx-1] = FALSE;
  }
}

/* 
 * NPM: Called through redraw_meters(), via draw_meters_and_peaks(), 
 * this handles updating gtk labels/colors related to updated peak values.
 * Note 'index' [0 to MULTI_TRACK_PEAK_CHANNELS-1] into global peak_label[]
 * which is initialized by envy24control.c:create_mixer_frame().
 */
static void draw_peak_labels(int index, int stereo, int peak1_level, int peak2_level) {
  GtkWidget* lbl;

  gtk_label_set_text(GTK_LABEL((stereo) ? peak_label[IDX_LMIX] : peak_label[index]),
		     peak_level_to_db(peak1_level)); /* put new value in label */
  if (peak1_level >= MAX_METERING_LEVEL) {			       /* if at 0dB, make label red; RESET reverts to normal color */
    lbl = (stereo) ? peak_label[IDX_LMIX] : peak_label[index];
    gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, peak_label_color);
  }

  /* NPM: Update "Analog Volume" panel's peak levels: the normal non-"stereo"
     case of single channel peak monitors of input or PCMs */
  if (!stereo) {		
    /* NPM: Update peak labels for DACs in "Analog Volume" panel */
    if ((index >= 0) && (index < envy_dac_volumes())) { /* index 0-8 corresponds to one of the eight DAC's */
      if ((index < MAX_OUTPUT_CHANNELS) /* make sure within bounds of dac_peak_label[] */
	  && (lbl = dac_peak_label[index]) != NULL) {
	gtk_label_set_text(GTK_LABEL(lbl), peak_level_to_db(peak1_level)); /* put new value in label */
	if (peak1_level >= MAX_METERING_LEVEL)
	  gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, peak_label_color);
      }
    }
    /* NPM: Update peak labels for ADCs in "Analog Volume" panel */
    else if ((index >= (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) /* ADC channels begin at 11 = MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS */
	     && (index <= (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS + envy_adc_volumes()))) { /* ADC channels end at 19 */
      if (((index - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) < MAX_INPUT_CHANNELS) /* make sure within bounds of adc_peak_label[] */
	  && (lbl = adc_peak_label[index - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)]) != NULL) {
	gtk_label_set_text(GTK_LABEL(lbl), peak_level_to_db(peak1_level)); /* put new value in label */
	if (peak1_level >= MAX_METERING_LEVEL)
	  gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, peak_label_color);
      }
    }
  }
  /* Handle special "stereo" case for right-channel of digital mixer */
  else if (peak_changed[IDX_RMIX]) { //for stereo, draw RMIX, but skip redraw if same
    gtk_label_set_text(GTK_LABEL(peak_label[IDX_RMIX]),
		       peak_level_to_db(peak2_level)); /* put new value in label */
    if (peak2_level >= MAX_METERING_LEVEL) /* if at 0dB, make label "selected"; RESET reverts to normal color */
      gtk_widget_modify_fg(peak_label[IDX_RMIX], GTK_STATE_NORMAL, peak_label_color);
  }
}

//NPM: below, fudging value 51.0 instead of 48.164799306 = 20*log10(1/256)
// or 48.130803609 = 20*log10(1/255) so as to offset meter in direction of
// scale markings.
#define METER_LEVEL(level, height)				\
  ((level==0)							\
   ? 0								\
   : ((int) (((double)(height))					\
	     * ((20.0 * log10(((double)(level))			\
			      / ((double)MAX_METERING_LEVEL))	\
		 + 51.0)				\
		/  51.0) ))-1                           \
   )

/* 
 * NPM: Called by redraw_meters(), this is the normal case 
 * where we draw the meter and peaks, if changed.
 */
static void draw_meters_and_peaks(int idx, int width, int height, int level1, int level2, 
				  int stereo, int segment_width) {
  int meter1 = (stereo)
    ? METER_LEVEL(peak_levels[IDX_LMIX], height)
    : METER_LEVEL(peak_levels[idx - 1],  height);
  int meter2 = (stereo)
    ? METER_LEVEL(peak_levels[IDX_RMIX], height)
    : 0;
  int peak1 = height - meter1;
  int peak2 = (stereo) ? height - meter2: 0;

  /*
   * draw the peak(s), only if changed
   */
  if ( (stereo && (peak_changed[IDX_LMIX] || peak_changed[IDX_RMIX]))
       || peak_changed[idx-1]) {
    int peak2_level, peak1_level
      = (stereo) ? peak_levels[IDX_LMIX] : peak_levels[idx-1];

    /* Draw the peak as a single line */
    gdk_draw_line(pixmap[idx],
		  GET_COLOR_FOR_PEAKLEVEL(peak1_level),
		  //X1
		  6,
		  //Y1
		  peak1 - 1,
		  //X2
		  5 + segment_width,
		  //Y2
		  peak1 - 1
		  );
    /* Handle special "stereo" case for right-channel of digital mixer */
    if (stereo && peak_changed[IDX_RMIX]) { //for stereo, draw RMIX, but skip redraw if same
      peak2_level = peak_levels[IDX_RMIX];
      gdk_draw_line(pixmap[idx],
		    GET_COLOR_FOR_PEAKLEVEL(peak2_level),
		    //X1
		    2 + (width / 2),
		    //Y1
		    peak2 - 1,
		    //X2
		    1 + (width / 2) + segment_width,
		    //Y2
		    peak2 - 1
		    );
    }
    else
      peak2_level = -1;	/* not used unless above case, which is also in draw_peak_labels()... but initialize anyways */

    draw_peak_labels(idx-1, stereo, peak1_level, peak2_level);

    /* reset peak_changed[] status now that new peak values rendered */
    if (stereo) {
      if (peak_changed[IDX_LMIX])
	peak_changed[IDX_LMIX] = FALSE;
      if (peak_changed[IDX_RMIX])
	peak_changed[IDX_RMIX] = FALSE;
    } 
    else {
      peak_changed[idx-1] = FALSE;
    }
  }

  /*
   * draw the meters
   * level1 at 0 --> no dspl
   * level1 at 1 --> draw rectange at (1/255)*height
   * level1 at 255 --> draw rectangle (255/255)*height
   */
  meter1 = METER_LEVEL(level1, height);
  if (stereo)
    meter2 = METER_LEVEL(level2, height);
  if (level1 != (stereo ? previous_levels[IDX_LMIX] : previous_levels[idx-1]) ) { //skip redraw if same
    gdk_draw_rectangle(pixmap[idx],
		       penBackground[idx],
		       TRUE,
		       //X
		       6,                               // draw black downward from peak
		       //Y
		       peak1,
		       //WIDTH
		       segment_width,
		       //HEIGHT
		       height - meter1 - peak1
		       );
    gdk_draw_rectangle(pixmap[idx],
		       penForeground[idx],
		       TRUE,
		       //X
		       6,
		       //Y
		       height - meter1,
		       //WIDTH
		       segment_width,
		       //HEIGHT
		       meter1
		       );                             
    /* save current level value, skip redraw next time if no change */
    if (stereo)
      previous_levels[IDX_LMIX] = level1;
    else
      previous_levels[idx-1] = level1;
  }
  if (stereo && (level2 != previous_levels[IDX_RMIX])) { //for stereo, draw RMIX, but skip redraw if same
    gdk_draw_rectangle(pixmap[idx],
		       penBackground[idx],
		       TRUE,
		       //X
		       2 + (width / 2),
		       //Y
		       peak2,
		       //WIDTH
		       segment_width,
		       //HEIGHT
		       height - meter2 - peak2
		       );
    gdk_draw_rectangle(pixmap[idx],
		       penForeground[idx],
		       TRUE,
		       //X
		       2 + (width / 2),
		       //Y
		       height - meter2,
		       //WIDTH
		       segment_width,
		       //HEIGHT
		       meter2
		       );
    /* save current level value, skip redraw next time if no change */
    previous_levels[IDX_RMIX] = level2; 
  }
}

static void redraw_meters(int idx, int width, int height, int level1, int level2) {
  int stereo = (idx == 0);
  int segment_width = (stereo)
    ? (width / 2) - 8
    : width - 12;

  if ( (stereo && ((peak_changed[IDX_LMIX] == RESET) || (peak_changed[IDX_RMIX] == RESET)))
       || peak_changed[idx-1] == RESET)	//needs full refresh, reset peaks button was clicked
    draw_meters_reset(idx, width, height, stereo, segment_width);
  else 
    draw_meters_and_peaks(idx, width, height, level1, level2, stereo, segment_width);
}

/* NPM: called out of level_meters_configure_event() at initialization to
 pickup foreground color for level metering from --lights_color
 command-line, or create default. */
static void levelmeters_init_fg(GtkWidget* widget, GdkGC *gc) {
  if (meter_fg == NULL) {	/* if --lights_color command-line argument not provided */
    meter_fg = (GdkColor *)g_malloc(sizeof(GdkColor)); /* free()'d on exit() */
    if (!gdk_color_parse("#1e90ff", meter_fg)) { /* "dodgerblue" per http://en.wikipedia.org/wiki/X11_color_names */
      free((void*) meter_fg); /* if for some impossible reason above fails, continue on valiantly */
      meter_fg = &(gtk_widget_get_style(widget)->text_aa[GTK_STATE_ACTIVE]); /* should never happen, but this would pick up color from existing gtk style w/o needing any addl color allocs */
    }
    else 
      gdk_color_alloc(gdk_colormap_get_system(), meter_fg);
  }
  gdk_gc_set_foreground(gc, meter_fg);
}

/* NPM: called out of level_meters_configure_event() at initialization to
 pickup background color for level metering from --bg_color command-line
 argument, or create default. */
static void levelmeters_init_bg(GtkWidget* widget, GdkGC *gc) {
  if (meter_bg == NULL) { /* if --bg_color command-line argument not provided */
    meter_bg = (GdkColor *)g_malloc(sizeof(GdkColor)); /* free()'d on exit() */
    if (!gdk_color_parse("#304050", meter_bg)) { /* a dark background color */
      free((void*) meter_bg); /* if for some impossible reason above fails, continue on valiantly */
      meter_bg = &(gtk_widget_get_style(widget)->text_aa[GTK_STATE_PRELIGHT]); /* should never happen, but this would pick up color from existing gtk style w/o needing any addl color allocs */
    }
    else 
      gdk_color_alloc(gdk_colormap_get_system(), meter_bg);
  }
  gdk_gc_set_foreground(gc, meter_bg); 
}

gint level_meters_configure_event(GtkWidget *widget, GdkEventConfigure *event) {
	int idx = get_index(gtk_widget_get_name(widget));
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);
	if (pixmap[idx] != NULL)
		gdk_pixmap_unref(pixmap[idx]);
	pixmap[idx] = gdk_pixmap_new(gtk_widget_get_window(widget),
				     allocation.width,
				     allocation.height,
				     -1);
	penWhiteLight[idx] = get_pen(idx, 0xffff, 0xffff, 0xffff);
	penGreenLight[idx] = get_pen(idx, 0, 0xffff, 0);

	/* NPM: Setup penForeground[idx] color for meters */
	levelmeters_init_fg(widget, (penForeground[idx] = gdk_gc_new(pixmap[idx])));
	/* NPM: Setup penBackground[idx] color for meters */
	levelmeters_init_bg(widget, (penBackground[idx] = gdk_gc_new(pixmap[idx])));

	penOrangeLight[idx] = get_pen(idx, 0xff11, 0x9911, 0);

	peak_label_color = (GdkColor *)g_malloc(sizeof(GdkColor)); /* free()'d on exit() */
        gdk_color_parse("red", peak_label_color);
	gdk_color_alloc(gdk_colormap_get_system(), peak_label_color);

	penRedLight[idx] = get_pen(idx, 0xffff, 0, 0);

	gdk_draw_rectangle(pixmap[idx],
			   gtk_widget_get_style(widget)->black_gc,
			   TRUE,
			   0, 0,
			   allocation.width,
			   allocation.height);

	/* NPM: ensure redraw_meters() below does a full refresh, per meter  */
	if (idx == 0) {		/* "if stereo" -- special case for L/R output pair of digital mixer */
	  // g_print("level_meters_configure_event() for stereo\n");
	  peak_levels[IDX_LMIX]     = 0;
	  peak_levels[IDX_RMIX]     = 0;
	  previous_levels[IDX_LMIX] = 0;
	  previous_levels[IDX_RMIX] = 0;
	  peak_changed[IDX_LMIX]    = RESET;
	  peak_changed[IDX_RMIX]    = RESET;
	}
	else {
	  // g_print("level_meters_configure_event() for %i\n", idx);
	  peak_levels[idx-1]        = 0;
	  previous_levels[idx-1]    = 0;
	  peak_changed[idx-1]       = RESET;
	}
	
	// g_print("configure: %i:%i\n", allocation.width, allocation.height);
	redraw_meters(idx, allocation.width, allocation.height, 0, 0);
	return TRUE;
}

gint level_meters_expose_event(GtkWidget *widget, GdkEventExpose *event) {
	int idx = get_index(gtk_widget_get_name(widget));
	int l1, l2;
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);
	
	get_levels(idx, &l1, &l2);
	redraw_meters(idx, allocation.width, allocation.height, l1, l2);
	gdk_draw_pixmap(gtk_widget_get_window(widget),
			gtk_widget_get_style(widget)->black_gc,
			pixmap[idx],
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	return FALSE;
}

gint level_meters_timeout_callback(gpointer data) {
	GtkWidget *widget;
	int idx, l1, l2;
	GtkAllocation allocation;

	update_peak_switch();
	for (idx = 0; idx <= pcm_output_channels; idx++) {
		get_levels(idx, &l1, &l2);
		widget = idx == 0 ? mixer_mix_drawing : mixer_drawing[idx-1];
		if (gtk_widget_get_visible(widget) && (pixmap[idx] != NULL)) {
			gtk_widget_get_allocation(widget, &allocation);
			redraw_meters(idx, allocation.width, allocation.height, l1, l2);
			gdk_draw_pixmap(gtk_widget_get_window(widget),
					gtk_widget_get_style(widget)->black_gc,
					pixmap[idx],
					0, 0,
					0, 0,
					allocation.width, allocation.height);
		}
		/* NPM: both cases below are special-case hack to get
		   "Analog Volume" PCM peak levels updating correctly,
		   should "Analog Volume" panel be selected before "Monitor
		   PCM outs" panel. In that situation,
		   "(gtk_widget_get_visible(widget) && (pixmap[idx] != NULL))"
		   fails as level_meters_configure_event() hasn't been
		   called yet (it gets called when user selects "Monitor
		   PCM outs" panel).
		*/
		else if ((idx != 0)
			 && ((idx-1) < envy_dac_volumes())
			 && (peak_changed[idx-1] == TRUE)) { 
		  draw_peak_labels(idx-1, FALSE, l1, l2);
		  peak_changed[idx-1] = FALSE;
		  // g_print("NPM PCM outputs hack calling draw_peak_labels(%i): %i\n", idx-1, l1);
		}
		/* NPM: make it work, even after user does a RESET, while
		   in "Analog Volume" panel, before "Monitor PCM outs"
		   created/configured */
		else if ((idx != 0)
			 && ((idx-1) < envy_dac_volumes())
			 && (peak_changed[idx-1] == RESET)) { /* note that get_levels() never retrieved l1, l2 due to unfulfilled RESET */
		  GtkWidget* lbl;
		  /* NPM: Reset colors of peak labels for DACs in "Analog Volume" panel */
		  if ((idx-1) < envy_dac_volumes()	/* index 0-8 corresponds to one of the eight DAC's */
		      && ((idx-1) < MAX_OUTPUT_CHANNELS) /* make sure within bounds of dac_peak_label[] */
		      && (lbl = dac_peak_label[idx-1]) != NULL)
		    gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, NULL); /* reset color */

		  /* NPM: Reset colors of peak labels for ADCs in "Analog Volume" panel */
		  else if (((idx-1) >= (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) /* ADC channels begin at 11 = MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS */
			   && ((idx-1) < (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS + envy_adc_volumes())) /* ADC channels end at 19 */
			   && (((idx-1) - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)) < MAX_INPUT_CHANNELS) /* make sure within bounds of adc_peak_label[] */
			   && (lbl = adc_peak_label[(idx-1) - (MAX_PCM_OUTPUT_CHANNELS+MAX_SPDIF_CHANNELS)]) != NULL)
		    gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL, NULL); /* reset color */

		  peak_changed[idx-1] = FALSE; /* hack -- force get_levels() to retrieve levels despite RESET */
		  get_levels(idx, &l1, &l2); 
		  draw_peak_labels(idx-1, FALSE, l1, l2);
		  // g_print("NPM PCM outputs RESET hack calling draw_peak_labels(%i): %i\n", idx-1, l1);
		}
	}
	if (view_spdif_playback) {
		for (idx = MAX_PCM_OUTPUT_CHANNELS + 1; idx <= MAX_OUTPUT_CHANNELS + spdif_channels; idx++) {
			get_levels(idx, &l1, &l2);
			widget = idx == 0 ? mixer_mix_drawing : mixer_drawing[idx-1];
			if (gtk_widget_get_visible(widget) && (pixmap[idx] != NULL)) {
				gtk_widget_get_allocation(widget, &allocation);
				redraw_meters(idx, allocation.width, allocation.height, l1, l2);
				gdk_draw_pixmap(gtk_widget_get_window(widget),
						gtk_widget_get_style(widget)->black_gc,
						pixmap[idx],
						0, 0,
						0, 0,
						allocation.width, allocation.height);
			}
		}
	}
	for (idx = MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + 1; idx <= input_channels + MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS; idx++) {
		get_levels(idx, &l1, &l2);
		widget = idx == 0 ? mixer_mix_drawing : mixer_drawing[idx-1];
		if (gtk_widget_get_visible(widget) && (pixmap[idx] != NULL)) {
			gtk_widget_get_allocation(widget, &allocation);
			redraw_meters(idx, allocation.width, allocation.height, l1, l2);
			gdk_draw_pixmap(gtk_widget_get_window(widget),
					gtk_widget_get_style(widget)->black_gc,
					pixmap[idx],
					0, 0,
					0, 0,
					allocation.width, allocation.height);
		}
	}
	for (idx = MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS + 1; \
		    idx <= spdif_channels + MAX_PCM_OUTPUT_CHANNELS + MAX_SPDIF_CHANNELS + MAX_INPUT_CHANNELS; idx++) {
		get_levels(idx, &l1, &l2);
		widget = idx == 0 ? mixer_mix_drawing : mixer_drawing[idx-1];
		if (gtk_widget_get_visible(widget) && (pixmap[idx] != NULL)) {
			gtk_widget_get_allocation(widget, &allocation);
			redraw_meters(idx, allocation.width, allocation.height, l1, l2);
			gdk_draw_pixmap(gtk_widget_get_window(widget),
					gtk_widget_get_style(widget)->black_gc,
					pixmap[idx],
					0, 0,
					0, 0,
					allocation.width, allocation.height);
		}
	}
	return TRUE;
}


/* NPM fixed lack of implementation ( https://bugzilla.redhat.com/show_bug.cgi?id=602903 )*/
void level_meters_reset_peaks(GtkButton *button, gpointer data) {
  int i;
  for (i = 0; i < MULTI_TRACK_PEAK_CHANNELS ; i++) {
    peak_levels[i]     = 0;
    previous_levels[i] = 0;
    peak_changed[i]    = RESET;
  }

  level_meters_timeout_callback((gpointer) data);
}

void level_meters_init(void) {
	int err;

	snd_ctl_elem_value_malloc(&peaks);
	snd_ctl_elem_value_set_interface(peaks, SND_CTL_ELEM_IFACE_PCM);
	snd_ctl_elem_value_set_name(peaks, "Multi Track Peak");
	if ((err = snd_ctl_elem_read(ctl, peaks)) < 0)
		/* older ALSA driver, using MIXER type */
		snd_ctl_elem_value_set_interface(peaks,
			SND_CTL_ELEM_IFACE_MIXER);
}

void level_meters_postinit(void) {
	level_meters_timeout_callback(NULL);
}
