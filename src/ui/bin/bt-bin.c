/* $Id$
 *
 * Buzztard
 * Copyright (C) 2010 Buzztard team <buzztard-devel@lists.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/* TODO:
 * - gst_bin_add(self,self->bin);
 * - need to flush thinks in sync with stae_change
 *
 * GST_DEBUG="*:3,bt*:4" gst-launch-0.10 -v filesrc location=$HOME/buzztard/share/buzztard/songs/303.bzt ! bt-bin ! fakesink
 * GST_DEBUG="*:3,bt*:4" gst-launch-0.10 -v filesrc location=$HOME/buzztard/share/buzztard/songs/303.bzt ! typefind ! bt-bin ! fakesink
 *

       filesrc gstfilesrc.c:984:gst_file_src_start:<filesrc0> opening file /home/ensonic/buzztard/share/buzztard/songs/303.bzt
       filesrc gstfilesrc.c:984:gst_file_src_start:<filesrc0> opening file /home/ensonic/buzztard/share/buzztard/songs/303.bzt
GST_SCHEDULING gstpad.c:4735:gst_pad_get_range:<filesrc0:src> getrange failed, flow: unexpected
GST_SCHEDULING gstpad.c:4848:gst_pad_pull_range:<typefindelement0:sink> pullrange failed, flow: unexpected
GST_SCHEDULING gstpad.c:4735:gst_pad_get_range:<typefindelement0:src> getrange failed, flow: unexpected
GST_SCHEDULING gstpad.c:4848:gst_pad_pull_range:<btbin0:sink> pullrange failed, flow: unexpected
        bt-bin bt-bin.c:68:bt_bin_load_song:<btbin0> input caps (NULL)
        bt-bin bt-bin.c:76:bt_bin_load_song:<btbin0> about to load buzztard song in audio/x-buzztard format
       bt-core song-io.c:181:bt_song_io_detect: detecting loader for file '', type 'audio/x-buzztard'
       bt-core song-io.c:188:bt_song_io_detect:   trying...
       bt-core song-io.c:188:bt_song_io_detect:   trying...
       bt-core song-io.c:304:bt_song_io_from_data: failed to detect type for media-type audio/x-buzztard

(gst-launch-0.10:29815): buzztard-CRITICAL **: bt_song_io_load: assertion `BT_IS_SONG_IO(self)' failed

 GST_DEBUG="*:3,bt*:4,*type*:4" gst-launch-0.10 -v -m filesrc location=$HOME/buzztard/share/buzztard/songs/303.bzt ! typefind ! fakesink
 GST_DEBUG="*:2,bt*:4,*type*:5,default:5" gst-launch-0.10 filesrc location=$HOME/buzztard/share/buzztard/songs/303.bzt ! typefind ! fakesink
 
 * gst-typefind $HOME/buzztard/share/buzztard/songs/303.bzt
 */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bt-bin.h"
#include <gio/gio.h>

#define GST_CAT_DEFAULT bt_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate bt_bin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate bt_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);


static GstBinClass *parent_class = NULL;

static gboolean
bt_bin_load_song (BtBin *self)
{
  gboolean res=FALSE;
  BtSongIO *loader=NULL;
  GstCaps *caps;
  GstStructure *s;
  const gchar *media_type = "audio/x-buzztard";
  guint len;
  gpointer data;
  
  caps = GST_PAD_CAPS (self->sinkpad);
  GST_INFO_OBJECT (self, "input caps %" GST_PTR_FORMAT, caps);
  if (caps) {
    if (!GST_CAPS_IS_SIMPLE (caps))
      goto Error;
    
    s = gst_caps_get_structure (caps,0);
    media_type = gst_structure_get_name (s);
  }
  GST_INFO_OBJECT (self, "about to load buzztard song in %s format", media_type);
  
  /* create song-loader */
  len = gst_adapter_available (self->adapter);
  data = (gpointer)gst_adapter_take (self->adapter, len);
  loader = bt_song_io_from_data (data, len, media_type);
  
  if (bt_song_io_load (loader, self->song)) {
    BtSetup *setup;
    BtMachine *machine;

    g_object_get (self->song,"setup",&setup,NULL);
    if((machine = bt_setup_get_machine_by_type (setup, BT_TYPE_SINK_MACHINE))) {
      BtSinkBin *sink_bin;
      GstPad *req_pad;

      g_object_get (machine,"machine",&sink_bin,NULL);
      //g_object_set (sink_bin, "mode", BT_SINK_BIN_MODE_PASS_THRU, NULL);

      req_pad = gst_element_get_request_pad (GST_ELEMENT (sink_bin), "src");
      gst_ghost_pad_set_target (GST_GHOST_PAD (self->srcpad),req_pad);
      res = TRUE;
    }
  }
  
Error:
  if (loader) {
    g_object_unref (loader);
  }
  return res;
}


static gboolean
bt_bin_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean res = FALSE;
  BtBin *self = BT_BIN (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (pad, "%s event received", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (self, "song loaded");
      /* parse the song */
      bt_bin_load_song (self);
      /* don't forward the event */
      gst_event_unref (event);
      break;
    default:
      res = gst_pad_push_event (self->srcpad, event);
      break;
  }
  
  gst_object_unref (pad);
  return res;
}

static GstFlowReturn
bt_bin_chain (GstPad * sinkpad, GstBuffer * buffer)
{
  BtBin *self = BT_BIN (GST_PAD_PARENT (sinkpad));
  
  GST_DEBUG_OBJECT (self, "loading song");
  
  /* push stuff in the adapter, we will start doing something in the sink event
   * handler when we get EOS */
  gst_adapter_push (self->adapter, buffer);

  return GST_FLOW_OK;
}

static void
bt_bin_loop (GstPad * sinkpad)
{
  BtBin *self = BT_BIN (GST_PAD_PARENT (sinkpad));
  GstFlowReturn ret;
  GstBuffer *buffer;

  GST_DEBUG_OBJECT (self, "loading song");

  ret = gst_pad_pull_range (self->sinkpad, self->offset, -1, &buffer);
  if (ret == GST_FLOW_UNEXPECTED) {
    GST_DEBUG_OBJECT (self, "song loaded");
    /* parse the song */
    bt_bin_load_song (self);
    goto pause;
  } else if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, (NULL), ("Unable to read song"));
    goto pause;
  } else {
    GST_DEBUG_OBJECT (self, "pushing buffer");
    gst_adapter_push (self->adapter, buffer);
    self->offset += GST_BUFFER_SIZE (buffer);
  }

  return;

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);
    GstEvent *event;

    GST_DEBUG_OBJECT (self, "pausing task, reason %s", reason);
    gst_pad_pause_task (sinkpad);
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      if (ret == GST_FLOW_UNEXPECTED) {
        /* perform EOS logic */
        event = gst_event_new_eos ();
        gst_pad_push_event (self->srcpad, event);
      } else {
        event = gst_event_new_eos ();
        /* for fatal errors we post an error message, post the error
         * first so the app knows about the error first. */
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            ("Internal data flow error."),
            ("streaming task paused, reason %s (%d)", reason, ret));
        gst_pad_push_event (self->srcpad, event);
      }
    }
  }
}

static gboolean
bt_bin_activate (GstPad * sinkpad)
{
  gboolean res;

  if (gst_pad_check_pull_range (sinkpad)) {
    res = gst_pad_activate_pull (sinkpad, TRUE);
    GST_INFO_OBJECT (sinkpad, "activating in pull mode: %d", res);
  } else {
    res = gst_pad_activate_push (sinkpad, TRUE);
    GST_INFO_OBJECT (sinkpad, "activating in push mode: %d", res);
  }
  return res;
}

static gboolean
bt_bin_activatepull (GstPad * pad, gboolean active)
{
  if (active) {
    return gst_pad_start_task (pad, (GstTaskFunction) bt_bin_loop, pad);
  } else {
    return gst_pad_stop_task (pad);
  }
}

static void
bt_bin_reset (BtBin *self)
{
  self->offset = 0;
  //self->discont = FALSE;
  /*
  GST_OBJECT_LOCK (self);
  if (self->song)
    xxx_close (self->song);
  self->song = NULL;
  GST_OBJECT_UNLOCK (self);
  */
  gst_adapter_clear (self->adapter);
}

static GstStateChangeReturn
bt_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  BtBin *self = BT_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      bt_bin_reset (self);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}


static void
bt_bin_dispose (GObject * object)
{
  BtBin *self = BT_BIN (object);

  g_object_unref (self->song);
  g_object_unref (self->app);
  g_object_unref (self->adapter);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
bt_bin_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&bt_bin_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&bt_bin_src_template));
  gst_element_class_set_details_simple (element_class, 
      "BtBin",
      "Codec/Decoder/Audio",
      "Buzztard song player",
      "Stefan Kost <ensonic@users.sf.net>");
}

static void
bt_bin_class_init (BtBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  parent_class = (GstBinClass *) g_type_class_peek_parent (klass);

  gobject_class->dispose  = bt_bin_dispose;

  gstelement_class->change_state = bt_bin_change_state;
}

static void
bt_bin_init (BtBin * self, BtBinClass * g_class)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (self);

  self->adapter = gst_adapter_new ();
  self->bin = GST_BIN (gst_bin_new ("song"));
  gst_bin_add (GST_BIN (self), GST_ELEMENT (self->bin));

  self->app = g_object_new (BT_TYPE_APPLICATION,"bin",self->bin,NULL);
  self->song = bt_song_new (self->app);
  
  
  self->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");
  gst_pad_set_activatepull_function (self->sinkpad, bt_bin_activatepull);
  gst_pad_set_activate_function (self->sinkpad, bt_bin_activate);
  gst_pad_set_event_function (self->sinkpad, bt_bin_sink_event);
  gst_pad_set_chain_function (self->sinkpad, bt_bin_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  // a ghostpad for sinkbins:src ghostpad
  self->srcpad = gst_ghost_pad_new_no_target ("src", GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}


static void
bt_bin_type_find (GstTypeFind * tf, gpointer ignore)
{
  gsize length = 16384;
  guint64 tf_length;
  guint8 *data;
  gchar *tmp,*mimetype;

  if ((tf_length = gst_type_find_get_length (tf)) > 0)
    length = MIN (length, tf_length);

  if ((data = gst_type_find_peek (tf, 0, length)) == NULL)
    return;
  
  // check it
  tmp = g_content_type_guess (NULL, data, length, NULL);
  if (tmp == NULL || g_content_type_is_unknown (tmp)) {
    g_free (tmp);
    return;
  }

  mimetype = g_content_type_get_mime_type (tmp);
  g_free (tmp);

  if (mimetype == NULL)
    return;

  GST_INFO ("Got mimetype '%s'", mimetype);
  
  //gst_type_find_suggest_simple (tf, GST_TYPE_FIND_LIKELY, "audio/x-buzztard", NULL);
  gst_type_find_suggest_simple (tf, GST_TYPE_FIND_LIKELY, mimetype, NULL);
}


GType bt_bin_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (!type)) {
    const GTypeInfo type_info = {
      sizeof (BtBinClass),
      (GBaseInitFunc)bt_bin_base_init,
      NULL,		          /* base_finalize */
      (GClassInitFunc)bt_bin_class_init,
      NULL,		          /* class_finalize */
      NULL,               /* class_data */
      sizeof (BtBin),
      0,                  /* n_preallocs */
      (GInstanceInitFunc) bt_bin_init
    };

    type = g_type_register_static (GST_TYPE_BIN, "BtBin", &type_info, (GTypeFlags) 0);
  }
  return type;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  const GList *plugins;
  BtSongIOModuleInfo *info;
  gchar **exts = NULL;
  guint i = 0, j ,l = 20;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "bt-bin", GST_DEBUG_FG_WHITE | GST_DEBUG_BG_BLACK, "buzztard song player");
  
  bt_init (NULL,NULL);
  
  plugins = bt_song_io_get_module_info_list ();
  exts = (gchar **)g_new (gpointer, l);
  for(;plugins; plugins = g_list_next (plugins)) {
    info=(BtSongIOModuleInfo *)plugins->data;
    j = 0;
    while(info->formats[j].name) {
      exts[i++] = (gchar *)info->formats[j].extension;
      j++;
      if (i >= l) {
        l *= 2;
        exts = (gchar **)g_renew (gpointer, (gpointer)exts, l);
      }
    }
  }
  exts[i] = NULL;
  
  gst_type_find_register (plugin, "audio/x-buzztard", GST_RANK_SECONDARY,
      bt_bin_type_find, exts, GST_CAPS_ANY, NULL, NULL);

  return gst_element_register (plugin, "bt-bin", GST_RANK_MARGINAL, BT_TYPE_BIN);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "buzztard-bin",
    "Buzztard song renderer",
    plugin_init,
    VERSION,
    "LGPL",
    PACKAGE_NAME,
    "http://www.buzztard.org");