/*
 * volume.c - analog volume settings
 *
 * This code is added by Takashi Iwai <tiwai@suse.de>
 *
 * Copyright (c) 2000 Jaroslav Kysela <perex@perex.cz>
 * Copyright (C) 2011 Tim E. Real (terminator356 on sourceforge)
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
// TER: For key defs.
#include <gdk/gdkkeysyms.h>
#include "envy24control.h"

#define toggle_set(widget, state) \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state);

#define DAC_VOLUME_NAME	"DAC Volume"
#define ADC_VOLUME_NAME	"ADC Volume"
#define IPGA_VOLUME_NAME "IPGA Analog Capture Volume"
#define DAC_SENSE_NAME	"Output Sensitivity Switch"
#define ADC_SENSE_NAME	"Input Sensitivity Switch"

static int dac_volumes;
//static int dac_max = 127; // TER
//static int adc_max = 127; // TER
static int adc_volumes;
static int ipga_volumes;
static int dac_senses;
static int adc_senses;
static int dac_sense_items;
static int adc_sense_items;
static char *dac_sense_name[4];
static char *adc_sense_name[4];
static const int mark_width = 7;  
static const int mark_height = 4; 
static const int mark_pad = 1;    
static char str_tmp[128];         
extern int input_channels, output_channels;

int envy_dac_volumes(void)
{
	return dac_volumes;
}

//int envy_dac_max(void)
//{
//	return dac_max;
//}

int envy_adc_volumes(void)
{
	return adc_volumes;
}

//int envy_adc_max(void)
//{
//	return adc_max;
//}

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

gboolean get_alsa_control_range(SliderScale *sl_scale, gdouble *min, gdouble *max) 
{
  const char *cname;
  switch(sl_scale->type)
  {
    case DAC_STRIP:
      cname = DAC_VOLUME_NAME;
    break;
    case ADC_STRIP:
      cname = ADC_VOLUME_NAME;
    break;
    case IPGA_STRIP:
      cname = IPGA_VOLUME_NAME;
    break;
    default:
      return FALSE;
  }
    
  snd_ctl_elem_info_t *elem_info;
  snd_ctl_elem_info_alloca(&elem_info);
  snd_ctl_elem_info_set_interface(elem_info, SND_CTL_ELEM_IFACE_MIXER);
  snd_ctl_elem_info_set_name(elem_info, cname);
  snd_ctl_elem_info_set_numid(elem_info, 0);
  snd_ctl_elem_info_set_index(elem_info, sl_scale->idx); 
  int err;
  if((err = snd_ctl_elem_info(ctl, elem_info)) < 0)
  {  
    g_print("get_alsa_control_range: Error reading control info: %s\n", snd_strerror(err));
    return FALSE;
  }  
  *min = (gdouble)snd_ctl_elem_info_get_min(elem_info);
  *max = (gdouble)snd_ctl_elem_info_get_max(elem_info);
  return TRUE;
} 

void scale_add_analog_marks(SliderScale     *sl_scale,
                            GtkPositionType  position,
                            gboolean         draw_legend_p)
{
  const char *cname;
  switch(sl_scale->type)
  {
    case DAC_STRIP:
      cname = DAC_VOLUME_NAME;
    break;
    case ADC_STRIP:
      cname = ADC_VOLUME_NAME;
    break;
    case IPGA_STRIP:
      cname = IPGA_VOLUME_NAME;
    break;
    default:
      return;
  }
    
  snd_ctl_elem_id_t *elem_id;
  snd_ctl_elem_id_alloca(&elem_id);
  snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
  snd_ctl_elem_id_set_name(elem_id, cname);
  snd_ctl_elem_id_set_index(elem_id, sl_scale->idx); 
  long dbminl, dbmaxl;
  if(snd_ctl_get_dB_range(ctl, elem_id, &dbminl, &dbmaxl) < 0)
    return;

  // Get the nearest max 6dB value below or equal.
  dbmaxl = (dbmaxl / 600) * 600;
  // Get the nearest min 6dB value above or equal.
  dbminl = (dbminl / 600) * 600;
  if((dbminl % 600) != 0)
    dbminl += 600;
  
  // 09/23/2011 TER. Crashes due to too many marks. Seems db range has now changed such that
  //  a very low value means -infinity. snd_ctl_get_dB_range returned a dbminl of -9999999, 
  //  which gives 16666 marks! So put some kind of limits: 
  if(dbminl < -12000) // Enough for say ten marks, down to -120db?
    dbminl = -12000;
  //if(dbmaxl > 6000) 
  //  dbmaxl = 6000;
  
  long i;
  long ival;
  long lastival;
  long first = 1;
  for(i = dbminl; i <= dbmaxl; i+= 600)
  {
    if(snd_ctl_convert_from_dB(ctl, elem_id, i, &ival, 0) < 0)
      continue;
    
    if(!first && ival == lastival)  // Keep going until we find a change.
      continue;
    first = 0;
    lastival = ival;
    
    if(draw_legend_p)
    {  
      const char *col;
      if(i > 0)
        col = "red";
      else if (i == 0) 
        col = "green";
      else 
        col = "blue";
      if(i <= -12000) // Say <= -120db = -infinity?
        sprintf(str_tmp, "<span color='%s' size='x-small'>~</span>", col);
      else
        sprintf(str_tmp, "<span color='%s' size='x-small'>%+ld</span>", col, i/100);
    }
    //printf("scale_add_analog_marks i:%d max:%d ival:%d\n", i, max, ival);
    
    scale_add_mark(sl_scale, (float)(-ival), position, draw_legend_p ? str_tmp : NULL);
  }
}

void scale_add_mark(SliderScale     *sl_scale,
                    gdouble          value,
                    GtkPositionType  position,
                    const gchar     *markup)
{
  ScaleMark *mark = g_new(ScaleMark, 1);
  mark->value = value;
  if(markup) 
    mark->markup = g_strdup(markup);
  else
    mark->markup = NULL;
  mark->position = position;
  sl_scale->marks = g_slist_prepend(sl_scale->marks, mark);
}

void scale_add_marks(GtkScale        *scale, 
                     SliderScale     *sl_scale,
                     GtkPositionType  position, 
                     gboolean         draw_legend_p) 
{
  switch(sl_scale->type)
  {
    case MIXER_STRIP:
      sl_scale->scale    = scale;
      if(no_scale_marks) 
        return;
  
      // We know it's an ice1712 envy24 chip. So we can hard-code these markings...
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE,
            position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>": NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+1*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-6</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+2*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-12</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+3*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-18</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+4*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-24</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+5*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-30</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+6*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-36</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+7*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-42</span>" : NULL);
      scale_add_mark(sl_scale, (float) MIN_MIXER_ATTENUATION_VALUE+8*MIXER_ATTENUATOR_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-48</span>" : NULL);
      //scale_add_mark(sl_scale, (float) LOW_MIXER_ATTENUATION_VALUE,
      //       position, draw_legend_p ? "<span color='blue' size='x-small'>off</span>" : NULL); 
    break;  

    // Ask ALSA for analog volume info, because the chip(s) may not be known, can't hard-code markings...
    case DAC_STRIP:
    case ADC_STRIP:
    case IPGA_STRIP:
      sl_scale->scale = scale;
      if(no_scale_marks) 
        return;
      scale_add_analog_marks(sl_scale, position, draw_legend_p);
    break;
    default:
      return;  
    
    /*
    case DAC_STRIP:
      sl_scale->scale = scale;

      scale_add_mark(sl_scale, (float) -envy_dac_max(),
            position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+1*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-6</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+2*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-12</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+3*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-18</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+4*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-24</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+5*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-30</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+6*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-36</span>" : NULL);
    //  scale_add_mark(sl_scale, (float) -envy_dac_max()+7*ANALOG_GAIN_STEP_SIZE,
    //         position, draw_legend_p ? "<span color='blue' size='x-small'>-42</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+8*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-48</span>" : NULL);
    //  scale_add_mark(sl_scale, (float) -envy_dac_max()+9*ANALOG_GAIN_STEP_SIZE,
    //         position, draw_legend_p ? "<span color='blue' size='x-small'>-54</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_dac_max()+10*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-60</span>" : NULL);
      //scale_add_mark(sl_scale, (float) 0,
      //      position, draw_legend_p ? "<span color='blue' size='x-small'>off</span>" : NULL); 
    break;  
    case ADC_STRIP:
      sl_scale->scale = scale;

      scale_add_mark(sl_scale, (float) -envy_adc_max(),
            position, draw_legend_p ? "<span color='red' size='x-small'>+18</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+1*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='red' size='x-small'>+12</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+2*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='red' size='x-small'>+6</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+3*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+4*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-6</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+5*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-12</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+6*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-18</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+7*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-24</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+8*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-30</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+9*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-36</span>" : NULL);
    //  scale_add_mark(sl_scale, (float) -envy_adc_max()+10*ANALOG_GAIN_STEP_SIZE,
    //         position, draw_legend_p ? "<span color='blue' size='x-small'>-42</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+11*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-48</span>" : NULL);
    //  scale_add_mark(sl_scale, (float) -envy_adc_max()+12*ANALOG_GAIN_STEP_SIZE,
    //         position, draw_legend_p ? "<span color='blue' size='x-small'>-54</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+13*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='blue' size='x-small'>-60</span>" : NULL);
      //scale_add_mark(sl_scale, (float) 0,
      //      position, draw_legend_p ? "<span color='blue' size='x-small'>off</span>" : NULL);
    break;  
    case IPGA_STRIP:
      sl_scale->scale = scale;

      scale_add_mark(sl_scale, (float) -envy_adc_max(),
            position, draw_legend_p ? "<span color='red' size='x-small'>+18</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+1*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='red' size='x-small'>+12</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+2*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='red' size='x-small'>+6</span>" : NULL);
      scale_add_mark(sl_scale, (float) -envy_adc_max()+3*ANALOG_GAIN_STEP_SIZE,
            position, draw_legend_p ? "<span color='green' size='x-small'>+0</span>" : NULL);
    break;  
    default:
      return;  
    */
    
  }  
}  

void scale_size_req_handler(GtkWidget *widget, GtkRequisition *requisition, gpointer data)
{
  //printf("scale_size_req_handler\n");
  
  SliderScale* sscale = (SliderScale*)data;
  gint w = 0, h = 0;
  
  if(sscale->marks)
  {  
    PangoLayout *layout;
    PangoRectangle layout_rect;
    GSList *m;
    
    layout = gtk_widget_create_pango_layout(widget, NULL);

    for(m = sscale->marks; m; m = m->next)
    {
      ScaleMark *mark = m->data;

      if(h + mark_height > h)
        h += mark_height;
      if(mark_width + mark_pad > w)
        w = mark_width + mark_pad;
        
      if(mark->markup)
      {
        pango_layout_set_markup(layout, mark->markup, -1);
        pango_layout_get_pixel_extents(layout, NULL, &layout_rect);
        
        if(h + layout_rect.height > h)
          h += layout_rect.height;
          
        //printf("scale_size_req_handler item w:%d\n", layout_rect.width);
  
        if(layout_rect.width + mark_width + mark_pad > w)
          w = layout_rect.width + mark_width + mark_pad;
      }
    }
    
    g_object_unref(layout);
  }
  
  //printf("scale_size_req_handler w:%d h:%d\n", w, h);

  requisition->width  = w; 
  requisition->height = h;
  
  return;
}

gboolean scale_expose_handler(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  //printf("scale_expose_handler\n");
  
  SliderScale* sscale = (SliderScale*)data;
  
  GtkStateType state_type;
  state_type = GTK_STATE_NORMAL;
  if(!gtk_widget_is_sensitive(widget))
    state_type = GTK_STATE_INSENSITIVE;

  if(sscale->marks)
  {  
    gint x1, x2, x3, y1, y2; 
    PangoLayout *layout;
    PangoRectangle layout_rect;
    GSList *m;

    GtkScale *scale = sscale->scale;
    GtkRange *rng = GTK_RANGE(scale);
    GtkAdjustment *adj = gtk_range_get_adjustment(rng);
    if(!adj)
      return FALSE;
    
    gdouble min = gtk_adjustment_get_lower(adj);
    gdouble max = gtk_adjustment_get_upper(adj);

    gint sl_w;
    gtk_widget_style_get(GTK_WIDGET(scale), "slider-length", &sl_w, NULL);
    
    //printf("scale_expose_handler sl_w:%d\n", sl_w);

    gdouble z = max - min;

    if(z == 0.0)
      return FALSE;
    
    // Extra space between top of slider thumb track and top of allocation. (Also at bottom).
    // Just a manual amount for now, measured on my machine. 
    // TODO: Make use of gtk 2.20 funcs for more accuracy, if available.
    gint h_extra = 2; 
    //gint slider_start = 0;
    //gint slider_end = 0;
    // These two funcs are new since gtk+-2.20 Try to avoid using them for now.
    //gtk_range_get_slider_range(rng, &slider_start, &slider_end);
    //GdkRectangle rng_rect;
    //gtk_range_get_range_rect(rng, &rng_rect);  
    
    gdouble h1 = (gdouble)(GTK_WIDGET(scale)->allocation.height - sl_w - 2*h_extra) / z;
    
    layout = gtk_widget_create_pango_layout(widget, NULL);

    //gdouble rtmp;
    
    for(m = sscale->marks; m; m = m->next)
    {
      ScaleMark *mark = m->data;

      if(mark->position == GTK_POS_LEFT)
      {
        x1 = widget->allocation.width - mark_width;
        x2 = widget->allocation.width - 1;
      }
      else
      {
        x1 = 0;
        x2 = mark_width - 1;
      }
      
      // These compile, but I get pre-c99 warning:
      //  " warning: incompatible implicit declaration of built-in function ‘round’ ".
      // Close visual inspection: Seems less accurate anyway, with round, than without.
      //y1 = (gint) round(mark->value * h1 + (gdouble)sl_w/2.0) + h_extra;
      //y1 = (gint) nearbyint(mark->value * h1 + (gdouble)sl_w/2.0) + h_extra;
      //y1 = lrint(mark->value * h1 + (gdouble)sl_w/2.0) + widget->allocation.y + h_extra;
      //y1 = (gint)(mark->value * h1 + (gdouble)sl_w/2.0) + widget->allocation.y + h_extra;
      //
      y1 = (gint)((mark->value - min) * h1 + (gdouble)sl_w/2.0) + h_extra;  // Works OK.
      //rtmp = (mark->value - min) * h1 + (gdouble)sl_w/2.0;
      //rtmp = (rtmp > 0.0) ? floor(rtmp + 0.5) : ceil(rtmp - 0.5);  // No C99 required.
      //y1 = (gint)rtmp + h_extra;
      
      gtk_paint_hline (widget->style, widget->window, state_type,
                        NULL, widget, "range-mark", x1, x2, y1);

      if(mark->markup)
      {  
        pango_layout_set_markup(layout, mark->markup, -1);
        pango_layout_get_pixel_extents(layout, NULL, &layout_rect);
      
        x3 = mark->position == GTK_POS_LEFT ? x1 - layout_rect.width - mark_pad : 
                                              mark_width + mark_pad; 

        y2 = y1 - layout_rect.height / 2;
        if(y2 < 0)
          y2 = 0;
        
        //gdk_draw_layout(layout);
        gtk_paint_layout(widget->style, widget->window, state_type,
                        FALSE, NULL, widget, "scale-mark", 
                        x3, y2, layout);
      }                  
    }
    
    g_object_unref(layout);
  }
  
  return TRUE;
}

//
// Event handler to give focus to a slider when scale area left-clicked.
//
gboolean scale_btpress_handler(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  //printf("scale_btpress_handler\n");

  if(event->button != 1) // 1 is normally left mouse button.
    return FALSE;
  SliderScale* sscale = (SliderScale*)data;
  if(!sscale->scale)
    return FALSE;
  gtk_widget_grab_focus(GTK_WIDGET(sscale->scale));
  return TRUE;
}

//
// Handle all types of slider scroll changes. 
gboolean slider_change_value_handler(GtkRange     *range,
                                     GtkScrollType scroll,
                                     gdouble       value,
                                     gpointer      data)
{
  SliderScale* sscale = (SliderScale*)data;
  GtkAdjustment *adj = gtk_range_get_adjustment(range);
  if(!adj)
    return FALSE;
  
  gboolean is_pg = TRUE;
  gdouble min   = gtk_adjustment_get_lower(adj);
  gdouble max   = gtk_adjustment_get_upper(adj);
  gdouble inc   = gtk_adjustment_get_page_increment(adj);
  gdouble curv  = gtk_adjustment_get_value(adj);
  
  // Tested, GtkScrollType values observed:
  // GTK_SCROLL_NONE
  // GTK_SCROLL_JUMP           // Drag the slider knob, or middle click trough.
  // GTK_SCROLL_STEP_BACKWARD  // Up key.
  // GTK_SCROLL_STEP_FORWARD   // Down key.
  // GTK_SCROLL_PAGE_BACKWARD  // Page up key, or left click upper trough.
  // GTK_SCROLL_PAGE_FORWARD   // Page down key, or left click lower trough.
  // GTK_SCROLL_STEP_UP       
  // GTK_SCROLL_STEP_DOWN     
  // GTK_SCROLL_PAGE_UP
  // GTK_SCROLL_PAGE_DOWN
  // GTK_SCROLL_STEP_LEFT
  // GTK_SCROLL_STEP_RIGHT
  // GTK_SCROLL_PAGE_LEFT
  // GTK_SCROLL_PAGE_RIGHT
  // GTK_SCROLL_START
  // GTK_SCROLL_END
  
  gboolean up = TRUE;
  switch(scroll)
  {
    case GTK_SCROLL_JUMP:
    {
      //printf("slider_change_value_handler GTK_SCROLL_JUMP value:%f\n", value); // Drag the slider knob, or middle click trough.
      
      // Round required.
      //value = round(value);  // C99 required
      value = (value > 0.0) ? floor(value + 0.5) : ceil(value - 0.5);
      
      // Clamp required.
      if(value < min)
        value = min;
      else
      if(value > max)
        value = max;
        
      gdouble newv = (gint)value;
      if(curv != newv)
      {  
        //gtk_adjustment_set_value(adj, newv);
        //gtk_range_set_value(range, newv);
        adj->value = newv;
        gtk_adjustment_value_changed(adj);
      }  
      return TRUE;
    }
    case GTK_SCROLL_STEP_BACKWARD:
      //printf("slider_change_value_handler GTK_SCROLL_STEP_BACKWARD\n"); // Up key.
      inc = adj->step_increment;
      is_pg = FALSE;
    break;
    case GTK_SCROLL_STEP_FORWARD:
      //printf("slider_change_value_handler GTK_SCROLL_STEP_FORWARD\n"); // Down key.
      inc = adj->step_increment;
      up = FALSE;
      is_pg = FALSE;
    break;
    case GTK_SCROLL_PAGE_BACKWARD:
      //printf("slider_change_value_handler GTK_SCROLL_PAGE_BACKWARD\n"); // Page up key, or left click upper trough.
    break;
    case GTK_SCROLL_PAGE_FORWARD:
      //printf("slider_change_value_handler GTK_SCROLL_PAGE_FORWARD\n"); // Page down key, or left click lower trough.
      up = FALSE;
    break;
    default:
      //printf("slider_change_value_handler: unhandled scroll type:%d\n", (int)scroll);
      return FALSE;
  }
  
  gdouble newv = up ? min : max;
  
  // If it's a page, and we want scale marks, and there are actually some marks, use them...
  if(is_pg && !no_scale_marks && sscale->marks)
  {  
    GSList *m;
    for(m = sscale->marks; m; m = m->next)
    {
      ScaleMark *mark = m->data;
      if(up)
      {  
        if(mark->value < curv && mark->value > newv)
          newv = mark->value;
      }  
      else
      if(mark->value > curv && mark->value < newv)
        newv = mark->value;
    }
    if(curv != newv)
    {  
      //gtk_adjustment_set_value(adj, newv);
      //gtk_range_set_value(range, newv);
      adj->value = newv;
      gtk_adjustment_value_changed(adj);
    }  
  }
  else
  // ...it's a step, or scale marks not wanted, or no scale marks. Use slider properties instead.
  {  
    //if(inc == 0.0)
    //  return FALSE;
    
    gdouble pg = (curv - min) / inc;
    gint pgint = (gint)pg;
      
    gdouble newv = pgint * inc + min;
    if(up)
    {
      // Page up. Are we already right on the page stop? Go to prev page.
      if(curv == newv)
        newv -= inc;
    }
    else
      // Page down. Go to next page no matter what.
      newv += inc;
      
    // Clamp required.
    if(newv < min)
      newv = min;
    else
    if(newv > max)
      newv = max;
      
    if(curv != newv)
    {  
      adj->value = newv;
      gtk_adjustment_value_changed(adj);
    }  
  }
  return TRUE;
}

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
  // TER: Stop jitter when adjusting sliders.
  //printf("dac_volume_update cur val:%f new val:%d\n", gtk_adjustment_get_value(GTK_ADJUSTMENT(av_dac_volume_adj[idx])), -snd_ctl_elem_value_get_integer(val, 0));
  if((int)gtk_adjustment_get_value(GTK_ADJUSTMENT(av_dac_volume_adj[idx])) != -snd_ctl_elem_value_get_integer(val, 0))
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
  // TER: Stop jitter when adjusting sliders.
  //printf("adc_volume_update cur val:%f new val:%d\n", GTK_ADJUSTMENT(av_adc_volume_adj[idx])->value, -snd_ctl_elem_value_get_integer(val, 0));
  if((int)gtk_adjustment_get_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx])) != -snd_ctl_elem_value_get_integer(val, 0))
  	gtk_adjustment_set_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx]),
				 -snd_ctl_elem_value_get_integer(val, 0));
	snd_ctl_elem_value_set_name(val, IPGA_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read ipga volume: %s\n", snd_strerror(err));
		return;
	}
	if (ipga_volumes > 0)
  {  
    // TER: Stop jitter when adjusting sliders.
    //printf("adc_volume_update ipga cur val:%f new val:%d\n", GTK_ADJUSTMENT(av_ipga_volume_adj[idx])->value, -0);
    //if((int)gtk_adjustment_get_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx])) != -0)
		//  gtk_adjustment_set_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx]),
		//			 -0);
    GtkAdjustment *adj = GTK_ADJUSTMENT(av_ipga_volume_adj[idx]);
    if((gint)adj->value != gtk_adjustment_get_upper(adj))
      gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
  }         
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
  // TER: Stop jitter when adjusting sliders.
  //printf("ipga_volume_update cur val:%f new val:%d\n", GTK_ADJUSTMENT(av_ipga_volume_adj[idx])->value, -ipga_vol);
  ipga_vol = snd_ctl_elem_value_get_integer(val, 0);
  if((int)gtk_adjustment_get_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx])) != -ipga_vol)
	  gtk_adjustment_set_value(GTK_ADJUSTMENT(av_ipga_volume_adj[idx]),
				 //-(ipga_vol = snd_ctl_elem_value_get_integer(val, 0)));
         -ipga_vol);
	snd_ctl_elem_value_set_name(val, ADC_VOLUME_NAME);
	snd_ctl_elem_value_set_index(val, idx);
	if ((err = snd_ctl_elem_read(ctl, val)) < 0) {
		g_print("Unable to read adc volume: %s\n", snd_strerror(err));
		return;
	}
	// set ADC volume to max if IPGA volume greater 0
	if (ipga_vol)
  {  
    // TER: Stop jitter when adjusting sliders.
    //printf("ipga_volume_update adc cur val:%f new val:%d\n", GTK_ADJUSTMENT(av_adc_volume_adj[idx])->value, -ipga_vol);
    //if((int)gtk_adjustment_get_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx])) != -adc_max)
		//  gtk_adjustment_set_value(GTK_ADJUSTMENT(av_adc_volume_adj[idx]),
		//			 -adc_max);
    GtkAdjustment *adj = GTK_ADJUSTMENT(av_adc_volume_adj[idx]);
    if((gint)adj->value != gtk_adjustment_get_lower(adj))
      gtk_adjustment_set_value(adj, gtk_adjustment_get_lower(adj));
  }         
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


// TER: For native GtkScale marks. Removed and replaced with custom drawing.
/*

//
//  NPM: "subroutine" used twice in create_mixer_frame() to draw markings
//  for each digital mixer input attenuation slider.  Each slider is drawn
//  optionally with or without a legend describing the dBs attenuation at
//  the given level.  Each slider controls attentuation of its input from
//  +0dB to -144.0dB (and "Off").
// 
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
		       position, NULL); // NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) 

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
//		       position, NULL); // NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) 
  }
}

// NPM: used in create_analog_volume() to draw dB markings on DAC attenuators 
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
		     position, NULL); // NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) 
  }
}

// NPM: used in create_analog_volume() to draw dB markings on ADC attenuators/amplifiers 
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
		     position, NULL); // NPM: last marker needs to not be a label, else the last level gets placed incorrectly (gtk2-2.18.9-3.fc12.x86_64 bug?) 
  }
}
*/

void dac_volume_adjust(GtkAdjustment *adj, gpointer data)
{
	int idx = (int)(long)data;
	snd_ctl_elem_value_t *val;
	int err; //, ival = -(int)adj->value; // TER 
  int ival = -(int)gtk_adjustment_get_value(adj);
  //printf("dac_volume_adjust cur val:%f new val:%d\n", gtk_adjustment_get_value(adj), ival);
  
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
    //snd_ctl_elem_id_clear(elem_id); 
	  snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
	  snd_ctl_elem_id_set_name(elem_id, DAC_VOLUME_NAME);
	  snd_ctl_elem_id_set_index(elem_id, idx); 
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
	int err; //, ival = -(int)adj->value; // TER
  int ival = -(int)gtk_adjustment_get_value(adj);
  
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
	  //snd_ctl_elem_id_clear(elem_id); 
	  snd_ctl_elem_id_set_interface(elem_id, SND_CTL_ELEM_IFACE_MIXER);
	  snd_ctl_elem_id_set_name(elem_id, ADC_VOLUME_NAME);
    snd_ctl_elem_id_set_index(elem_id, idx); 
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
	int err;// , ival = -(int)adj->value; // TER
  int ival = gtk_adjustment_get_value(adj);
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
		//dac_max = snd_ctl_elem_info_get_max(info); // TER
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
		//adc_max = snd_ctl_elem_info_get_max(info); // TER
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
