/* $Id: sink-machine.c,v 1.20 2004-10-22 12:01:06 ensonic Exp $
 * class for a sink machine
 */
 
#define BT_CORE
#define BT_SINK_MACHINE_C

#include <libbtcore/core.h>
#include <libbtcore/sink-machine.h>
#include <libbtcore/machine-private.h>

struct _BtSinkMachinePrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;
};

static BtMachineClass *parent_class=NULL;

//-- constructor methods

/**
 * bt_sink_machine_new:
 * @song: the song the new instance belongs to
 * @id: the id, we can use to lookup the machine
 * @plugin_name: the name of the gst-plugin the machine is using
 *
 * Create a new instance
 * A machine should be added to a songs setup using
 * <code>#bt_setup_add_machine(setup,BT_MACHINE(machine));</code>.
 *
 * Returns: the new instance or NULL in case of an error
 */
BtSinkMachine *bt_sink_machine_new(const BtSong *song, const gchar *id) {
  BtSinkMachine *self;
  BtSettings *settings;
  gchar *audiosink_name,*system_audiosink_name;
  gchar *plugin_name;
  
  // @todo get plugin_name from settings
  g_object_get(settings,"audiosink",&audiosink_name,"system-audiosink",&system_audiosink_name,NULL);
  if(is_string(audiosink_name)) plugin_name=audiosink_name;
  else if(is_string(system_audiosink_name)) plugin_name=system_audiosink_name;
  else {
     GST_ERROR("no audiosink configured");
     goto Error;
  }

  g_assert(BT_IS_SONG(song));
  g_assert(id);
  
  if(!(self=BT_SINK_MACHINE(g_object_new(BT_TYPE_SINK_MACHINE,"song",song,"id",id,"plugin-name",plugin_name,NULL)))) {
    goto Error;
  }
  if(!bt_machine_new(BT_MACHINE(self))) {
    goto Error;
  }
  g_free(system_audiosink_name);
  g_free(audiosink_name);
  g_object_try_unref(settings);
  return(self);
Error:
  g_free(system_audiosink_name);
  g_object_try_unref(settings);
  g_object_try_unref(self);
  return(NULL);
}

//-- methods

//-- wrapper

//-- class internals

/* returns a property for the given property_id for this object */
static void bt_sink_machine_get_property(GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  BtSinkMachine *self = BT_SINK_MACHINE(object);
  return_if_disposed();
  switch (property_id) {
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

/* sets the given properties for this object */
static void bt_sink_machine_set_property(GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BtSinkMachine *self = BT_SINK_MACHINE(object);
  return_if_disposed();
  switch (property_id) {
    default: {
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void bt_sink_machine_dispose(GObject *object) {
  BtSinkMachine *self = BT_SINK_MACHINE(object);
	return_if_disposed();
  self->priv->dispose_has_run = TRUE;

  GST_DEBUG("!!!! self=%p",self);
  if(G_OBJECT_CLASS(parent_class)->dispose) {
    (G_OBJECT_CLASS(parent_class)->dispose)(object);
  }
}

static void bt_sink_machine_finalize(GObject *object) {
  BtSinkMachine *self = BT_SINK_MACHINE(object);

  GST_DEBUG("!!!! self=%p",self);

  g_free(self->priv);

  if(G_OBJECT_CLASS(parent_class)->finalize) {
    (G_OBJECT_CLASS(parent_class)->finalize)(object);
  }
}

static void bt_sink_machine_init(GTypeInstance *instance, gpointer g_class) {
  BtSinkMachine *self = BT_SINK_MACHINE(instance);
  self->priv = g_new0(BtSinkMachinePrivate,1);
  self->priv->dispose_has_run = FALSE;
}

static void bt_sink_machine_class_init(BtSinkMachineClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	BtMachineClass *base_class = BT_MACHINE_CLASS(klass);

  parent_class=g_type_class_ref(BT_TYPE_MACHINE);
  
  gobject_class->set_property = bt_sink_machine_set_property;
  gobject_class->get_property = bt_sink_machine_get_property;
  gobject_class->dispose      = bt_sink_machine_dispose;
  gobject_class->finalize     = bt_sink_machine_finalize;
}

GType bt_sink_machine_get_type(void) {
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (BtSinkMachineClass),
      NULL, // base_init
      NULL, // base_finalize
      (GClassInitFunc)bt_sink_machine_class_init, // class_init
      NULL, // class_finalize
      NULL, // class_data
      sizeof (BtSinkMachine),
      0,   // n_preallocs
	    (GInstanceInitFunc)bt_sink_machine_init, // instance_init
    };
		type = g_type_register_static(BT_TYPE_MACHINE,"BtSinkMachine",&info,0);
  }
  return type;
}

