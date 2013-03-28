/* Buzztard
 * Copyright (C) 2006 Buzztard team <buzztard-devel@lists.sf.net>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#define BT_CORE
#define BT_TOOLS_C

#include <math.h>
#include <time.h>
#include <sys/resource.h>

#include "core_private.h"

//-- gst registry

static gboolean
bt_gst_registry_class_filter (GstPluginFeature * const feature,
    gpointer user_data)
{
  const gchar **categories = (const gchar **) user_data;
  gboolean res = FALSE;

  if (GST_IS_ELEMENT_FACTORY (feature)) {
    const gchar *klass =
        gst_element_factory_get_metadata (GST_ELEMENT_FACTORY (feature),
        GST_ELEMENT_METADATA_KLASS);
    res = TRUE;
    while (res && *categories) {
      GST_LOG ("  %s", *categories);
      // there is also strcasestr()
      res = (strstr (klass, *categories++) != NULL);
    }
  }
  return (res);
}

/**
 * bt_gst_registry_get_element_factories_matching_all_categories:
 * @class_filter: path for filtering (e.g. "Sink/Audio")
 *
 * Iterates over all available elements and filters by categories given in
 * @class_filter.
 *
 * Returns: list of element factories, use gst_plugin_feature_list_free() after
 * use.
 *
 * Since: 0.6
 */
GList *
bt_gst_registry_get_element_factories_matching_all_categories (const gchar *
    class_filter)
{

  g_return_val_if_fail (BT_IS_STRING (class_filter), NULL);

  GST_DEBUG ("run filter for categories: %s", class_filter);

  gchar **categories = g_strsplit (class_filter, "/", 0);
  GList *list = gst_registry_feature_filter (gst_registry_get (),
      bt_gst_registry_class_filter, FALSE, (gpointer) categories);

  GST_DEBUG ("filtering done");
  g_strfreev (categories);
  return (list);
}


/**
 * bt_gst_registry_get_element_names_matching_all_categories:
 * @class_filter: path for filtering (e.g. "Sink/Audio")
 *
 * Iterates over all available elements and filters by categories given in
 * @class_filter.
 *
 * Returns: list of read-only element names, g_list_free after use.
 */
GList *
bt_gst_registry_get_element_names_matching_all_categories (const gchar *
    class_filter)
{
  const GList *node;
  GList *res = NULL;

  g_return_val_if_fail (BT_IS_STRING (class_filter), NULL);

  GST_DEBUG ("run filter for categories: %s", class_filter);

  GList *const list =
      bt_gst_registry_get_element_factories_matching_all_categories
      (class_filter);

  GST_DEBUG ("filtering done");

  for (node = list; node; node = g_list_next (node)) {
    GstPluginFeature *const feature = GST_PLUGIN_FEATURE (node->data);
    res =
        g_list_prepend (res, (gpointer) gst_plugin_feature_get_name (feature));
  }
  gst_plugin_feature_list_free ((GList *) list);
  return (res);
}

/**
 * bt_gst_element_factory_get_pad_template:
 * @factory: element factory
 * @name: name of the pad-template, e.g. "src" or "sink_%u"
 *
 * Lookup a pad template by name.
 *
 * Returns: the pad template or %NULL if not found
 */
GstPadTemplate *
bt_gst_element_factory_get_pad_template (GstElementFactory * factory,
    const gchar * name)
{
  /* get pad templates */
  const GList *list = gst_element_factory_get_static_pad_templates (factory);
  for (; list; list = g_list_next (list)) {
    GstStaticPadTemplate *t = (GstStaticPadTemplate *) list->data;

    GST_INFO_OBJECT (factory, "Checking pad-template '%s'", t->name_template);
    if (g_strcmp0 (t->name_template, name) == 0) {
      return gst_static_pad_template_get (t);
    }
  }
  GST_WARNING_OBJECT (factory, "No pad-template '%s'", name);
  return NULL;
}

/**
 * bt_gst_element_factory_can_sink_media_type:
 * @factory: element factory to check
 * @name: caps type name
 *
 * Check if the sink pads of the given @factory are compatible with the given
 * @name. The @name can e.g. be "audio/x-raw".
 *
 * Returns: %TRUE if the pads are compatible.
 */
gboolean
bt_gst_element_factory_can_sink_media_type (GstElementFactory * factory,
    const gchar * name)
{
  GList *node;
  GstStaticPadTemplate *tmpl;
  GstCaps *caps;
  guint i, size;
  const GstStructure *s;

  g_assert (GST_IS_ELEMENT_FACTORY (factory));

  for (node = (GList *) gst_element_factory_get_static_pad_templates (factory);
      node; node = g_list_next (node)) {
    tmpl = node->data;
    if (tmpl->direction == GST_PAD_SINK) {
      caps = gst_static_caps_get (&tmpl->static_caps);
      GST_INFO ("  testing caps: %" GST_PTR_FORMAT, caps);
      if (gst_caps_is_any (caps)) {
        gst_caps_unref (caps);
        return (TRUE);
      }
      size = gst_caps_get_size (caps);
      for (i = 0; i < size; i++) {
        s = gst_caps_get_structure (caps, i);
        if (gst_structure_has_name (s, name)) {
          gst_caps_unref (caps);
          return (TRUE);
        }
      }
      gst_caps_unref (caps);
    } else {
      GST_DEBUG ("  skipping template, wrong dir: %d", tmpl->direction);
    }
  }
  return (FALSE);
}

/**
 * bt_gst_check_elements:
 * @list: a #GList with element names
 *
 * Check if the given elements exist.
 *
 * Returns: a list of element-names which do not exist, %NULL if all elements exist
 */
GList *
bt_gst_check_elements (GList * list)
{
  GList *res = NULL, *node;
  GstRegistry *registry;
  GstPluginFeature *feature;

  g_return_val_if_fail (gst_is_initialized (), NULL);

  if ((registry = gst_registry_get ())) {
    for (node = list; node; node = g_list_next (node)) {
      if ((feature =
              gst_registry_lookup_feature (registry,
                  (const gchar *) node->data))) {
        gst_object_unref (feature);
      } else {
        res = g_list_prepend (res, node->data);
      }
    }
  }
  return (res);
}

/**
 * bt_gst_check_core_elements:
 *
 * Check if all core elements exist.
 *
 * Returns: a list of elements that does not exist, %NULL if all elements exist.
 * The list is static, don't free.
 */
GList *
bt_gst_check_core_elements (void)
{
  static GList *core_elements = NULL;
  static GList *res = NULL;
  static gboolean core_elements_checked = FALSE;

  /* TODO(ensonic): if registry ever gets a 'changed' signal, we need to connect to that and
   * reset core_elements_checked to FALSE
   * There is gst_registry_get_feature_list_cookie() now
   */
  if (!core_elements_checked) {
    // gstreamer
    core_elements = g_list_prepend (core_elements, "capsfilter");
    core_elements = g_list_prepend (core_elements, "fdsrc");
    core_elements = g_list_prepend (core_elements, "fdsink");
    core_elements = g_list_prepend (core_elements, "filesink");
    core_elements = g_list_prepend (core_elements, "identity");
    core_elements = g_list_prepend (core_elements, "queue");
    core_elements = g_list_prepend (core_elements, "tee");
    // gst-plugins-base
    core_elements = g_list_prepend (core_elements, "audioconvert");
    core_elements = g_list_prepend (core_elements, "audioresample");
    core_elements = g_list_prepend (core_elements, "audiotestsrc");
    core_elements = g_list_prepend (core_elements, "adder");
    core_elements = g_list_prepend (core_elements, "volume");
    core_elements_checked = TRUE;
    res = bt_gst_check_elements (core_elements);
    g_list_free (core_elements);
    core_elements = res;
  } else {
    res = core_elements;
  }
  return (res);
}

//-- gst safe linking

/*
 * bt_gst_get_peer_pad:
 * @elem: a gstreamer element
 *
 * Get the peer pad connected to the first pad in the given iter.
 *
 * Returns: the pad or %NULL
 */
static GstPad *
bt_gst_get_peer_pad (GstIterator * it)
{
  gboolean done = FALSE;
  GValue item = { 0, };
  GstPad *pad, *peer_pad = NULL;

  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:
        pad = GST_PAD (g_value_get_object (&item));
        peer_pad = gst_pad_get_peer (pad);
        done = TRUE;
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }
  g_value_unset (&item);
  gst_iterator_free (it);
  return (peer_pad);
}

#if 0
/*
 * bt_gst_element_get_src_peer_pad:
 * @elem: a gstreamer element
 *
 * Get the peer pad connected to the given elements first source pad.
 *
 * Returns: the pad or %NULL
 */
static GstPad *
bt_gst_element_get_src_peer_pad (GstElement * const elem)
{
  return (bt_gst_get_peer_pad (gst_element_iterate_src_pads (elem)));
}
#endif

/*
 * bt_gst_element_get_sink_peer_pad:
 * @elem: a gstreamer element
 *
 * Get the peer pad connected to the given elements first sink pad.
 *
 * Returns: the pad or %NULL
 */
static GstPad *
bt_gst_element_get_sink_peer_pad (GstElement * const elem)
{
  return (bt_gst_get_peer_pad (gst_element_iterate_sink_pads (elem)));
}

static GstPadProbeReturn
async_add (GstPad * tee_src, GstPadProbeInfo * info, gpointer user_data)
{
  GList *analyzers = (GList *) user_data;
  GstPad *analyzer_sink =
      gst_element_get_static_pad (GST_ELEMENT (analyzers->data), "sink");
  const GList *node;
  GstElement *next;
  GstPadLinkReturn plr;

  GST_INFO_OBJECT (tee_src, "sync state");
  for (node = analyzers; node; node = g_list_next (node)) {
    next = GST_ELEMENT (node->data);
    if (!(gst_element_sync_state_with_parent (next))) {
      GST_INFO_OBJECT (tee_src, "cannot sync state for elements \"%s\"",
          GST_OBJECT_NAME (next));
    }
  }

  GST_INFO_OBJECT (tee_src, "linking to tee");
  if (GST_PAD_LINK_FAILED (plr = gst_pad_link (tee_src, analyzer_sink))) {
    GST_INFO_OBJECT (tee_src, "cannot link analyzers to tee");
    GST_WARNING_OBJECT (tee_src, "%s", bt_gst_debug_pad_link_return (plr,
            tee_src, analyzer_sink));
  }
  gst_object_unref (analyzer_sink);
  g_list_free (analyzers);

  GST_INFO_OBJECT (tee_src, "add analyzers done");

  return GST_PAD_PROBE_REMOVE;
}

/**
 * bt_bin_activate_tee_chain:
 * @bin: the bin
 * @tee: the tee to connect the chain to
 * @analyzers: the list of analyzers
 * @is_playing: whether the pipeline is streaming data
 *
 * Add the elements from @analyzers to the @bin and link them. Handle pad
 * blocking in playing mode.
 *
 * Return: %TRUE for success
 */
gboolean
bt_bin_activate_tee_chain (GstBin * bin, GstElement * tee, GList * analyzers,
    gboolean is_playing)
{
  gboolean res = TRUE;
  GstElementClass *tee_class = GST_ELEMENT_GET_CLASS (tee);
  GstPad *tee_src;
  const GList *node;
  GstElement *prev = NULL, *next;

  g_return_val_if_fail (GST_IS_BIN (bin), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (tee), FALSE);
  g_return_val_if_fail (analyzers, FALSE);

  GST_INFO_OBJECT (bin, "add analyzers (%d elements)",
      g_list_length (analyzers));

  // do final link afterwards
  for (node = analyzers; (node && res); node = g_list_next (node)) {
    next = GST_ELEMENT (node->data);

    if (!(res = gst_bin_add (bin, next))) {
      GST_INFO_OBJECT (bin, "cannot add element \"%s\" to bin",
          GST_OBJECT_NAME (next));
      return FALSE;
    }
    if (prev) {
      if (!(res = gst_element_link (prev, next))) {
        GST_INFO_OBJECT (bin, "cannot link elements \"%s\" -> \"%s\"",
            GST_OBJECT_NAME (prev), GST_OBJECT_NAME (next));
        return FALSE;
      }
    }
    prev = next;
  }
  GST_INFO_OBJECT (bin, "blocking tee.src");
  tee_src = gst_element_request_pad (tee,
      gst_element_class_get_pad_template (tee_class, "src_%u"), NULL, NULL);
  analyzers = g_list_copy (analyzers);
  if (is_playing) {
    gst_pad_add_probe (tee_src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, async_add,
        analyzers, NULL);
  } else {
    async_add (tee_src, NULL, analyzers);
  }
  return (res);
}

static GstPadProbeReturn
async_remove (GstPad * tee_src, GstPadProbeInfo * info, gpointer user_data)
{
  GList *analyzers = (GList *) user_data;
  GstElement *tee = (GstElement *) gst_pad_get_parent (tee_src);
  GstBin *bin = (GstBin *) gst_element_get_parent (tee);
  const GList *node;
  GstElement *prev, *next;
  GstStateChangeReturn state_ret;

  prev = tee;
  for (node = analyzers; node; node = g_list_next (node)) {
    next = GST_ELEMENT (node->data);

    if ((state_ret =
            gst_element_set_state (next,
                GST_STATE_NULL)) != GST_STATE_CHANGE_SUCCESS) {
      GST_INFO ("cannot set state to NULL for element '%s', ret='%s'",
          GST_OBJECT_NAME (next),
          gst_element_state_change_return_get_name (state_ret));
    }
    gst_element_unlink (prev, next);
    prev = next;
  }
  for (node = analyzers; node; node = g_list_next (node)) {
    next = GST_ELEMENT (node->data);
    if (!gst_bin_remove (bin, next)) {
      GST_INFO ("cannot remove element '%s' from bin", GST_OBJECT_NAME (next));
    }
  }

  GST_INFO ("releasing request pad for tee");
  gst_element_release_request_pad (tee, tee_src);
  gst_object_unref (tee_src);
  gst_object_unref (tee);
  gst_object_unref (bin);
  g_list_free (analyzers);

  return GST_PAD_PROBE_REMOVE;
}

/**
 * bt_bin_deactivate_tee_chain:
 * @bin: the bin
 * @tee: the tee to connect the chain to
 * @analyzers: the list of analyzers
 * @is_playing: wheter the pipeline is streaming data
 *
 * Add the elements from @analyzers to the @bin and link them. Handle pad
 * blocking in playing mode.
 *
 * Return: %TRUE for success
 */
gboolean
bt_bin_deactivate_tee_chain (GstBin * bin, GstElement * tee, GList * analyzers,
    gboolean is_playing)
{
  gboolean res = FALSE;
  GstPad *tee_src;

  g_return_val_if_fail (GST_IS_BIN (bin), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (tee), FALSE);
  g_return_val_if_fail (analyzers, FALSE);

  GST_INFO ("remove analyzers (%d elements)", g_list_length (analyzers));

  if ((tee_src =
          bt_gst_element_get_sink_peer_pad (GST_ELEMENT (analyzers->data)))) {
    analyzers = g_list_copy (analyzers);
    // block src_pad (of tee)
    if (is_playing) {
      gst_pad_add_probe (tee_src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
          async_remove, analyzers, NULL);
    } else {
      async_remove (tee_src, NULL, analyzers);
    }
    res = TRUE;
  }
  return res;
}

//-- gst debugging

/**
 * bt_gst_debug_pad_link_return:
 * @link_res: pad link result
 * @src_pad: the source pad
 * @sink_pad: the sink pad
 *
 * Format a nice debug message from failed pad links.
 *
 * Returns: the message. The returned string has to be used before the can be
 * called again, otherwise the previous reult will be overwritten.
 */
const gchar *
bt_gst_debug_pad_link_return (GstPadLinkReturn link_res, GstPad * src_pad,
    GstPad * sink_pad)
{
  static gchar msg1[5000];
  gchar msg2[4000];
  static gchar *link_res_desc[] = {
    "link succeeded",
    "pads have no common grandparent",
    "pad was already linked",
    "pads have wrong direction",
    "pads do not have common format",
    "pads cannot cooperate in scheduling",
    "refused for some reason"
  };

  if (!src_pad || !sink_pad) {
    /* one of the pads is NULL */
    msg2[0] = '\0';
  } else {
    switch (link_res) {
      case GST_PAD_LINK_WRONG_HIERARCHY:{
        GstObject *src_parent = GST_OBJECT_PARENT (src_pad);
        GstObject *sink_parent = GST_OBJECT_PARENT (sink_pad);
        sprintf (msg2, " : parent(src) = %s, parent(sink) = %s",
            (src_parent ? GST_OBJECT_NAME (src_parent) : "(NULL)"),
            (sink_parent ? GST_OBJECT_NAME (sink_parent) : "(NULL)"));
        break;
      }
      case GST_PAD_LINK_WAS_LINKED:{
        GstPad *src_peer = src_pad->peer;
        GstPad *sink_peer = sink_pad->peer;
        sprintf (msg2, " : peer(src) = %s:%s, peer(sink) = %s:%s",
            GST_DEBUG_PAD_NAME (src_peer), GST_DEBUG_PAD_NAME (sink_peer));
        break;
      }
      case GST_PAD_LINK_WRONG_DIRECTION:{
        static gchar *dir_name[] = { "unknown", "src", "sink" };
        sprintf (msg2, " : direction(src) = %s, direction(sink) = %s",
            ((src_pad->direction <
                    3) ? dir_name[src_pad->direction] : "invalid"),
            ((sink_pad->direction <
                    3) ? dir_name[sink_pad->direction] : "invalid"));
        break;
      }
      case GST_PAD_LINK_NOFORMAT:{
        GstCaps *srcc = gst_pad_query_caps (src_pad, NULL);
        GstCaps *sinkc = gst_pad_query_caps (sink_pad, NULL);
        gchar *src_caps = gst_caps_to_string (srcc);
        gchar *sink_caps = gst_caps_to_string (sinkc);
        sprintf (msg2, " : caps(src) = %s, caps(sink) = %s", src_caps,
            sink_caps);
        g_free (src_caps);
        g_free (sink_caps);
        gst_caps_unref (srcc);
        gst_caps_unref (sinkc);
        break;
      }
      case GST_PAD_LINK_NOSCHED:
      case GST_PAD_LINK_REFUSED:
      case GST_PAD_LINK_OK:
      default:
        msg2[0] = '\0';
        break;
    }
  }

  sprintf (msg1, "%s:%s -> %s:%s : %s%s",
      GST_DEBUG_PAD_NAME (src_pad), GST_DEBUG_PAD_NAME (sink_pad),
      link_res_desc[-link_res], msg2);

  return msg1;
}

//-- gst element messages

/**
 * bt_gst_level_message_get_aggregated_field:
 * @structure: the message structure
 * @field_name: the field, such as 'decay' or 'peak'
 * @default_value: a default, in the case of inf/nan levels
 *
 * Aggregate the levels per channel and return the averaged level.
 *
 * Returns: the average level field for all channels
 */
gdouble
bt_gst_level_message_get_aggregated_field (const GstStructure * structure,
    const gchar * field_name, gdouble default_value)
{
  const GValue *values;
  guint i, size;
  gdouble sum = 0.0;
  GValueArray *arr;

  values = gst_structure_get_value (structure, field_name);
  arr = (GValueArray *) g_value_get_boxed (values);
  size = arr->n_values;
  for (i = 0; i < size; i++) {
    sum += g_value_get_double (g_value_array_get_nth (arr, i));
  }
  if (G_UNLIKELY (isinf (sum) || isnan (sum))) {
    //GST_WARNING("level.%s was INF or NAN, %lf",field_name,sum);
    return default_value;
  }
  return (sum / size);
}

/**
 * bt_gst_analyzer_get_waittime:
 * @analyzer: the analyzer
 * @structure: the message data
 * @endtime_is_running_time: some elements (level) report endtime as running
 *   time and therefore need segment correction
 *
 * Get the time to wait for audio corresponding to the analyzed data to be
 * rendered.
 *
 * Returns: the wait time in ns.
 */
GstClockTime
bt_gst_analyzer_get_waittime (GstElement * analyzer,
    const GstStructure * structure, gboolean endtime_is_running_time)
{
  GstClockTime timestamp, duration;
  GstClockTime waittime = GST_CLOCK_TIME_NONE;

  if (gst_structure_get_clock_time (structure, "running-time", &timestamp)
      && gst_structure_get_clock_time (structure, "duration", &duration)) {
    /* wait for middle of buffer */
    waittime = timestamp + (duration / 2);
  } else if (gst_structure_get_clock_time (structure, "endtime", &timestamp)) {
    /* level send endtime as stream_time and not as running_time */
    if (endtime_is_running_time) {
      waittime =
          gst_segment_to_running_time (&GST_BASE_TRANSFORM (analyzer)->segment,
          GST_FORMAT_TIME, timestamp);
    } else {
      waittime = timestamp;
    }
  }
  return waittime;
}

//-- gst compat

//-- glib compat & helper

/**
 * bt_g_type_get_base_type:
 * @type: a GType
 *
 * Call g_type_parent() as long as it returns a parent.
 *
 * Returns: the super parent type, aka base type.
 */
GType
bt_g_type_get_base_type (GType type)
{
  GType base;

  while ((base = g_type_parent (type)))
    type = base;
  return (type);
}


//-- cpu load monitoring

static GstClockTime treal_last = G_GINT64_CONSTANT (0), tuser_last =
G_GINT64_CONSTANT (0), tsys_last = G_GINT64_CONSTANT (0);
//long clk=1;
//long num_cpus;

#if WIN32
#include <windows.h>            //For GetSystemInfo
#endif


/*
 * bt_cpu_load_init:
 *
 * Initializes cpu usage monitoring.
 */
static void GST_GNUC_CONSTRUCTOR
bt_cpu_load_init (void)
{
  treal_last = gst_util_get_timestamp ();
  //clk=sysconf(_SC_CLK_TCK);

  /* TODO(ensonic): we might need to handle multi core cpus and divide by num-cores
   * http://lxr.linux.no/linux/include/linux/cpumask.h
   #ifndef WIN32
   num_cpus = sysconf (_SC_NPROCESSORS_ONLN);
   #else
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   num_cpus = si.dwNumberOfProcessors;
   #endif
   */
}

/**
 * bt_cpu_load_get_current:
 *
 * Determines the current CPU load.
 *
 * Returns: CPU usage as integer ranging from 0% to 100%
 */
guint
bt_cpu_load_get_current (void)
{
  struct rusage rus;
  GstClockTime tnow, treal, tuser, tsys;
  guint cpuload;

  // check real time elapsed
  tnow = gst_util_get_timestamp ();
  treal = tnow - treal_last;
  treal_last = tnow;
  // check time spent in our process
  getrusage (RUSAGE_SELF, &rus);
  /* children are child processes, which we don't have 
   * getrusage(RUSAGE_CHILDREN,&ruc); */
  tnow = GST_TIMEVAL_TO_TIME (rus.ru_utime);
  tuser = tnow - tuser_last;
  tuser_last = tnow;
  tnow = GST_TIMEVAL_TO_TIME (rus.ru_stime);
  tsys = tnow - tsys_last;
  tsys_last = tnow;
  // percentage
  cpuload =
      (guint) gst_util_uint64_scale (tuser + tsys, G_GINT64_CONSTANT (100),
      treal);
  GST_LOG ("real %" GST_TIME_FORMAT ", user %" GST_TIME_FORMAT ", sys %"
      GST_TIME_FORMAT " => cpuload %d", GST_TIME_ARGS (treal),
      GST_TIME_ARGS (tuser), GST_TIME_ARGS (tsys), cpuload);
  //printf("user %6.4lf, sys %6.4lf\n",(double)tms.tms_utime/(double)clk,(double)tms.tms_stime/(double)clk);

  return (cpuload);
}
