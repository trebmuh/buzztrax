// $Id: song.c,v 1.79 2005-07-27 17:16:04 ensonic Exp $
/**
 * SECTION:btsong
 * @short_description: class of a song project object (contains #BtSongInfo, 
 * #BtSetup, #BtSequence and #BtWavetable)
 *
 * A song is the top-level container object to manage all song-related objects.
 */ 

#define BT_CORE
#define BT_SONG_C

#include <libbtcore/core.h>

//-- signal ids

enum {
  PLAY_EVENT,
  STOP_EVENT,
  LAST_SIGNAL
};

//-- property ids

enum {
  SONG_APP=1,
	SONG_BIN,
  SONG_MASTER,
  SONG_SONG_INFO,
  SONG_SEQUENCE,
  SONG_SETUP,
	SONG_WAVETABLE,
	SONG_UNSAVED
};

struct _BtSongPrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;
  
	BtSongInfo*  song_info; 
	BtSequence*  sequence;
	BtSetup*     setup;
	BtWavetable* wavetable;
	
	/* whenever the song is changed, unsave should be set TRUE */
	gboolean unsaved;

  /* the application that currently uses the song */
  BtApplication *app;
	/* the main gstreamer container element */
	GstBin *bin;
  /* the element that has the clock */
  BtSinkMachine *master;
};

static GObjectClass *parent_class=NULL;

static guint signals[LAST_SIGNAL]={0,};

//-- constructor methods

/**
 * bt_song_new:
 * @app: the application object the songs belongs to.
 *
 * Create a new instance.
 * The new song instance automatically has one instance of #BtSetup, #BtSequence
 * and #BtSongInfo. These instances can be retrieved via the respecting
 * properties. 
 *
 * For example use following code to retrive a BtSequence from the song class:
 * <informalexample><programlisting language="c">
 * BtSequence *sequence;
 * ...
 * g_object_get(BT_SONG(song), "sequence", &amp;sequence, NULL);</programlisting>
 * </informalexample>
 *
 * Returns: the new instance or %NULL in case of an error
 */
BtSong *bt_song_new(const BtApplication *app) {
  BtSong *self=NULL;
  GstBin *bin;
	
  g_return_val_if_fail(BT_IS_APPLICATION(app),NULL);
  
  g_object_get(G_OBJECT(app),"bin",&bin,NULL);
  self=BT_SONG(g_object_new(BT_TYPE_SONG,"app",app,"bin",bin,NULL));
  g_object_try_unref(bin);
	GST_INFO("  new song created: %p",self);
  return(self);
}

//-- methods

/**
 * bt_song_set_unsaved:
 * @self: the song that should be changed
 * @unsaved: the new state of the songs unsaved flag
 *
 * Use this method instead of directly setting the state via g_object_set() to
 * avoid double notifies, if the state is unchanged.
 */
void bt_song_set_unsaved(const BtSong *self,gboolean unsaved) {
	if(self->priv->unsaved!=unsaved) {
		self->priv->unsaved=unsaved;
		g_object_notify(G_OBJECT(self),"unsaved");
	}
}

/**
 * bt_song_play:
 * @self: the song that should be played
 *
 * Starts to play the specified song instance from beginning.
 * This methods emits the "play" signal.
 *
 * Returns: %TRUE for success
 */
gboolean bt_song_play(const BtSong *self) {
  gboolean res;

	g_return_val_if_fail(BT_IS_SONG(self),FALSE);

  // emit signal that we start playing
  g_signal_emit(G_OBJECT(self), signals[PLAY_EVENT], 0);
  if(!(res=bt_sequence_play(self->priv->sequence))) {
		GST_WARNING("playing song failed");
	}
  // emit signal that we have finished playing
  g_signal_emit(G_OBJECT(self), signals[STOP_EVENT], 0);
  return(res);
}

/**
 * bt_song_stop:
 * @self: the song that should be stopped
 *
 * Stops the playback of the specified song instance.
 *
 * Returns: %TRUE for success
 */
gboolean bt_song_stop(const BtSong *self) {
  gboolean res;

  g_return_val_if_fail(BT_IS_SONG(self),FALSE);
  
  res=bt_sequence_stop(self->priv->sequence);
  return(res);
}

/**
 * bt_song_pause:
 * @self: the song that should be paused
 *
 * Pauses the playback of the specified song instance
 *
 * Returns: %TRUE for success
 */
gboolean bt_song_pause(const BtSong *self) {
  g_assert(BT_IS_SONG(self));
  // @todo remember play position
  // @todo sequence playing stuff needs to be updated to support pause/continue
  return(gst_element_set_state(GST_ELEMENT(self->priv->bin),GST_STATE_PAUSED)!=GST_STATE_FAILURE);
}

/**
 * bt_song_continue:
 * @self: the song that should be paused
 *
 * Continues the playback of the specified song instance
 *
 * Returns: %TRUE for success
 */
gboolean bt_song_continue(const BtSong *self) {
  g_assert(BT_IS_SONG(self));
  // @todo reuse play position
  // @todo sequence playing stuff needs to be updated to support pause/continue
  return(gst_element_set_state(GST_ELEMENT(self->priv->bin),GST_STATE_PLAYING)!=GST_STATE_FAILURE);
}

/**
 * bt_song_write_to_xml_file:
 * @self: the song that should be written
 *
 * To aid debugging applications one can use this method to write out the whole
 * network of gstreamer elements that form the song into an XML file.
 * This XML file can be loaded into gst-editor.
 */
void bt_song_write_to_xml_file(const BtSong *self) {
  FILE *out;
  g_assert(BT_IS_SONG(self));
  
	// @todo find a way to not overwrite files during a run
  if((out=fopen("/tmp/buzztard-song.xml","wb"))) {
    gst_xml_write_file(GST_ELEMENT(self->priv->bin),out);
    fclose(out);
  }
}

//-- wrapper

//-- class internals

/* returns a property for the given property_id for this object */
static void bt_song_get_property(GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  BtSong *self = BT_SONG(object);
  return_if_disposed();
  switch (property_id) {
    case SONG_APP: {
      g_value_set_object(value, self->priv->app);
    } break;
    case SONG_BIN: {
      g_value_set_object(value, self->priv->bin);
    } break;
    case SONG_MASTER: {
      g_value_set_object(value, self->priv->master);
    } break;
    case SONG_SONG_INFO: {
      g_value_set_object(value, self->priv->song_info);
    } break;
    case SONG_SEQUENCE: {
      g_value_set_object(value, self->priv->sequence);
    } break;
    case SONG_SETUP: {
      g_value_set_object(value, self->priv->setup);
    } break;
    case SONG_WAVETABLE: {
      g_value_set_object(value, self->priv->wavetable);
    } break;
    case SONG_UNSAVED: {
      g_value_set_boolean(value, self->priv->unsaved);
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

/* sets the given properties for this object */
static void bt_song_set_property(GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BtSong *self = BT_SONG(object);
  return_if_disposed();
  switch (property_id) {
    case SONG_APP: {
      g_object_try_weak_unref(self->priv->app);
      self->priv->app = BT_APPLICATION(g_value_get_object(value));
			g_object_try_weak_ref(self->priv->app);
      //GST_DEBUG("set the app for the song: %p",self->priv->app);
    } break;
		case SONG_BIN: {
      g_object_try_unref(self->priv->bin);
			self->priv->bin=GST_BIN(g_value_dup_object(value));
      GST_DEBUG("set the bin for the song: %p",self->priv->bin);
		} break;
		case SONG_MASTER: {
      g_object_try_weak_unref(self->priv->master);
			self->priv->master = BT_SINK_MACHINE(g_value_get_object(value));
      g_object_try_weak_ref(self->priv->master);
      GST_DEBUG("set the master for the song: %p",self->priv->master);
		} break;
    case SONG_UNSAVED: {
      self->priv->unsaved = g_value_get_boolean(value);
      GST_DEBUG("set the unsaved flag for the song: %d",self->priv->unsaved);
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void bt_song_dispose(GObject *object) {
  BtSong *self = BT_SONG(object);

	return_if_disposed();
  self->priv->dispose_has_run = TRUE;
	
	//DEBUG
	bt_song_write_to_xml_file(self);
	//DEBUG

  GST_DEBUG("!!!! self=%p",self);

  g_object_try_weak_unref(self->priv->master);
	g_object_try_unref(self->priv->song_info);
	g_object_try_unref(self->priv->sequence);
	g_object_try_unref(self->priv->setup);
	g_object_try_unref(self->priv->wavetable);
	g_object_try_unref(self->priv->bin);
	g_object_try_weak_unref(self->priv->app);

	GST_DEBUG("  chaining up");
  if(G_OBJECT_CLASS(parent_class)->dispose) {
    (G_OBJECT_CLASS(parent_class)->dispose)(object);
  }
	GST_DEBUG("  done");
}

static void bt_song_finalize(GObject *object) {
  BtSong *self = BT_SONG(object);
  
  GST_DEBUG("!!!! self=%p",self);
  
  g_free(self->priv);

	GST_DEBUG("  chaining up");
  if(G_OBJECT_CLASS(parent_class)->finalize) {
    (G_OBJECT_CLASS(parent_class)->finalize)(object);
  }
	GST_DEBUG("  done");
}

static void bt_song_init(GTypeInstance *instance, gpointer g_class) {
  BtSong *self = BT_SONG(instance);
	
  GST_DEBUG("song_init self=%p",self);
  self->priv = g_new0(BtSongPrivate,1);
  self->priv->dispose_has_run = FALSE;
  self->priv->song_info = bt_song_info_new(self);
  self->priv->sequence  = bt_sequence_new(self);
  self->priv->setup     = bt_setup_new(self);
  self->priv->wavetable = bt_wavetable_new(self);
	self->priv->unsaved   = FALSE;
	GST_DEBUG("  done");
}

static void bt_song_class_init(BtSongClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
 
  parent_class=g_type_class_ref(G_TYPE_OBJECT);
 
  gobject_class->set_property = bt_song_set_property;
  gobject_class->get_property = bt_song_get_property;
  gobject_class->dispose      = bt_song_dispose;
  gobject_class->finalize     = bt_song_finalize;

  klass->play_event = NULL;
  klass->stop_event = NULL;

  /** 
	 * BtSong::play:
   * @self: the song object that emitted the signal
	 *
	 * signals that this song has started to play
	 */
  signals[PLAY_EVENT] = g_signal_new("play",
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                        G_ABS_STRUCT_OFFSET(BtSongClass,play_event),
                                        NULL, // accumulator
                                        NULL, // acc data
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, // return type
                                        0 // n_params
                                        );
  
  /** 
	 * BtSong::stop:
   * @self: the song object that emitted the signal
	 *
	 * signals that this song has finished to play
	 */
  signals[STOP_EVENT] = g_signal_new("stop",
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                        G_ABS_STRUCT_OFFSET(BtSongClass,stop_event),
                                        NULL, // accumulator
                                        NULL, // acc data
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, // return type
                                        0 // n_params
                                        );

  g_object_class_install_property(gobject_class,SONG_APP,
                                  g_param_spec_object("app",
                                     "app contruct prop",
                                     "set application object, the song belongs to",
                                     BT_TYPE_APPLICATION, /* object type */
                                     G_PARAM_CONSTRUCT_ONLY |G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,SONG_BIN,
																	g_param_spec_object("bin",
                                     "bin construct prop",
                                     "songs top-level GstElement container",
                                     GST_TYPE_BIN, /* object type */
                                     G_PARAM_CONSTRUCT_ONLY |G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,SONG_MASTER,
																	g_param_spec_object("master",
                                     "master prop",
                                     "songs audio_sink",
                                     BT_TYPE_SINK_MACHINE, /* object type */
                                     G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,SONG_SONG_INFO,
																	g_param_spec_object("song-info",
                                     "song-info prop",
                                     "songs metadata sub object",
                                     BT_TYPE_SONG_INFO, /* object type */
                                     G_PARAM_READABLE));

  g_object_class_install_property(gobject_class,SONG_SEQUENCE,
																	g_param_spec_object("sequence",
                                     "sequence prop",
                                     "songs sequence sub object",
                                     BT_TYPE_SEQUENCE, /* object type */
                                     G_PARAM_READABLE));

  g_object_class_install_property(gobject_class,SONG_SETUP,
																	g_param_spec_object("setup",
                                     "setup prop",
                                     "songs setup sub object",
                                     BT_TYPE_SETUP, /* object type */
                                     G_PARAM_READABLE));

  g_object_class_install_property(gobject_class,SONG_WAVETABLE,
																	g_param_spec_object("wavetable",
                                     "wavetable prop",
                                     "songs wavetable sub object",
                                     BT_TYPE_WAVETABLE, /* object type */
                                     G_PARAM_READABLE));

  g_object_class_install_property(gobject_class,SONG_UNSAVED,
                                  g_param_spec_boolean("unsaved",
                                     "unsaved prop",
                                     "tell wheter the current state of the song has been saved",
                                     TRUE,
                                     G_PARAM_READWRITE));
}

/**
 * bt_song_get_type:
 *
 * Returns: the type of #BtSong
 */
GType bt_song_get_type(void) {
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      G_STRUCT_SIZE(BtSongClass),
      NULL, // base_init
      NULL, // base_finalize
      (GClassInitFunc)bt_song_class_init, // class_init
      NULL, // class_finalize
      NULL, // class_data
      G_STRUCT_SIZE(BtSong),
      0,   // n_preallocs
	    (GInstanceInitFunc)bt_song_init, // instance_init
			NULL // value_table
    };
		type = g_type_register_static(G_TYPE_OBJECT,"BtSong",&info,0);
  }
  return type;
}
