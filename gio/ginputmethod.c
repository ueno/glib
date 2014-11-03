/*
 * Copyright © 2014 Daiki Ueno
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Daiki Ueno <dueno@src.gnome.org>
 */

#include "config.h"

#include "ginputmethod.h"
#include "gdbus-inputmethod-generated.h"
#include "glibintl.h"

/**
 * SECTION:ginputmethod
 * @title: GInputMethod
 * @short_description: A #GApplication implementation for input method services
 * @include: gio/gio.h
 *
 * A #GInputMethod is an implementation of #GApplication, specialized
 * for input method services.  It manages creation of
 * #GInputMethodEngine, which is responsible for the actual message
 * handling.
 */

/**
 * GInputMethodEngineClass:
 * @key_event: invoked on the engine instance when it receives a key event.
 * @focus: invoked on the engine instance when it receives a focus event.
 * @reset: invoked on the engine instance when the application resets
 *     the status.
 * @destroy: invoked on the engine instance when the application no longer
 *     needs the engine.
 * @set_surrounding_text: invoked on the engine instance when the application
 *     sets the surrounding text.
 * @set_content_type: invoked on the engine instance when the application
 *     has changed the content-type.
 * @commit: invoked on the engine instance when a completed characters are
 *     sent to the application.
 * @preedit_changed: invoked on the engine instance when a change of preedit
 *     is sent to the application.
 * @delete_surrounding_text: invoked on the engine instance to request
 *     deletion of the surrounding text.
 *
 * Virtual function table for #GInputMethodEngine.
 */

struct _GInputMethodEnginePrivate
{
  _GFreedesktopInputMethodEngine *skeleton;
  gchar *client_id;
};

enum
  {
    ENGINE_PROP_NONE,
    ENGINE_PROP_CLIENT_ID
  };

enum
{
  ENGINE_SIGNAL_KEY_EVENT,
  ENGINE_SIGNAL_FOCUS,
  ENGINE_SIGNAL_RESET,
  ENGINE_SIGNAL_DESTROY,
  ENGINE_SIGNAL_SET_SURROUNDING_TEXT,
  ENGINE_SIGNAL_SET_CONTENT_TYPE,
  ENGINE_SIGNAL_COMMIT,
  ENGINE_SIGNAL_PREEDIT_CHANGED,
  ENGINE_SIGNAL_DELETE_SURROUNDING_TEXT,
  NR_ENGINE_SIGNALS
};

static guint g_input_method_engine_signals[NR_ENGINE_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (GInputMethodEngine, g_input_method_engine,
                            G_TYPE_OBJECT)

static void
g_input_method_engine_real_commit (GInputMethodEngine *engine,
                                   const gchar        *text)
{
  _g_freedesktop_input_method_engine_emit_commit (engine->priv->skeleton, text);
}

static void
g_input_method_engine_real_preedit_changed (GInputMethodEngine  *engine,
                                            const gchar         *text,
                                            GInputMethodStyling *styling,
                                            gsize                n_styling,
                                            gint                 cursor_pos)
{
  GVariantBuilder builder;
  gsize i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(uuu)"));
  for (i = 0; i < n_styling; i++)
    g_variant_builder_add (&builder, "(uuu)",
                           styling[i].start,
                           styling[i].end,
                           styling[i].type);

  _g_freedesktop_input_method_engine_emit_preedit_changed (engine->priv->skeleton,
                                                           text,
                                                           g_variant_builder_end (&builder),
                                                           cursor_pos);
}

static void
g_input_method_engine_real_delete_surrounding_text (GInputMethodEngine  *engine,
                                                    gint                 offset,
                                                    guint                nchars)
{
  _g_freedesktop_input_method_engine_emit_delete_surrounding_text (engine->priv->skeleton,
                                                                   offset,
                                                                   nchars);
}

static void
g_input_method_engine_real_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (object);

  switch (prop_id)
    {
    case ENGINE_PROP_CLIENT_ID:
      engine->priv->client_id = g_value_dup_string (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_input_method_engine_real_dispose (GObject *object)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (object);

  g_clear_pointer (&engine->priv->client_id, g_free);

  if (engine->priv->skeleton)
    {
      GDBusInterfaceSkeleton *skeleton
        = G_DBUS_INTERFACE_SKELETON (engine->priv->skeleton);
      GDBusConnection *connection
        = g_dbus_interface_skeleton_get_connection (skeleton);
      if (connection)
        g_dbus_interface_skeleton_unexport_from_connection (skeleton,
                                                            connection);
      engine->priv->skeleton = NULL;
    }

  G_OBJECT_CLASS (g_input_method_engine_parent_class)->dispose (object);
}

static gboolean
key_event_accumulator (GSignalInvocationHint *ihint,
                       GValue                *return_accu,
                       const GValue          *handler_return,
                       gpointer               data)
{
  gboolean handled;

  handled = g_value_get_boolean (handler_return);
  if (handled)
    {
      g_value_set_boolean (return_accu, handled);
      return FALSE;
    }

  return TRUE;
}

static void
g_input_method_engine_class_init (GInputMethodEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->commit = g_input_method_engine_real_commit;
  klass->preedit_changed = g_input_method_engine_real_preedit_changed;
  klass->delete_surrounding_text
    = g_input_method_engine_real_delete_surrounding_text;

  object_class->set_property = g_input_method_engine_real_set_property;
  object_class->dispose = g_input_method_engine_real_dispose;

  g_object_class_install_property (object_class, ENGINE_PROP_CLIENT_ID,
    g_param_spec_string ("client-id",
                         P_("Client ID"),
                         P_("The ID of a client which created the engine"),
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GInputMethodEngine::key-event:
   *
   * The ::key-event signal is emitted on the engine instance when it
   * receives a key event.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_KEY_EVENT] =
    g_signal_new ("key-event", G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, key_event),
                  key_event_accumulator,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_BOOLEAN,
                  2,
                  G_TYPE_UINT,
                  G_TYPE_BOOLEAN);

  /**
   * GInputMethodEngine::focus:
   *
   * The ::focus signal is emitted on the engine instance when it
   * receives a focus event.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_FOCUS] =
    g_signal_new ("focus", G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, focus),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  /**
   * GInputMethodEngine::reset:
   *
   * The ::reset signal is emitted on the engine instance when the
   * application resets the status.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_RESET] =
    g_signal_new ("reset", G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, reset),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * GInputMethodEngine::destroy:
   *
   * The ::destroy signal is emitted on the engine instance when the
   * application no longer needs the engine.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_DESTROY] =
    g_signal_new ("destroy", G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, destroy),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * GInputMethodEngine::set-surrounding-text:
   *
   * The ::set-surrounding-text signal is emitted on the engine
   * instance when the application sets the surrounding text.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_SET_SURROUNDING_TEXT] =
    g_signal_new ("set-surrounding-text",
                  G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, set_surrounding_text),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_UINT,
                  G_TYPE_UINT);

  /**
   * GInputMethodEngine::set-content-type:
   *
   * The ::set-content-type signal is emitted on the engine instance
   * when the application has changed the content-type.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_SET_CONTENT_TYPE] =
    g_signal_new ("set-content-type",
                  G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, set_content_type),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_UINT,
                  G_TYPE_UINT);

  /**
   * GInputMethodEngine::commit:
   *
   * The ::commit signal is emitted on the engine instance when a
   * completed characters are sent to the application.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_COMMIT] =
    g_signal_new ("commit", G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, commit),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  /**
   * GInputMethodEngine::preedit-changed:
   *
   * The ::preedit-changed signal is emitted on the engine instance
   * when a change of preedit is sent to the application.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_PREEDIT_CHANGED] =
    g_signal_new ("preedit-changed", G_TYPE_INPUT_METHOD_ENGINE,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, preedit_changed),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_STRING,
                  G_TYPE_POINTER,
                  G_TYPE_UINT,
                  G_TYPE_UINT);

  /**
   * GInputMethodEngine::delete-surrounding-text:
   *
   * The ::delete-surrounding-text signal is emitted on the engine
   * instance to request deletion of the surrounding text.
   */
  g_input_method_engine_signals[ENGINE_SIGNAL_DELETE_SURROUNDING_TEXT] =
    g_signal_new ("delete-surrounding-text",
                  G_TYPE_INPUT_METHOD_ENGINE, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodEngineClass, delete_surrounding_text),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_UINT);
}

static gboolean
on_handle_key_event (_GFreedesktopInputMethodEngine *object,
                     GDBusMethodInvocation          *invocation,
                     guint                           arg_keycode,
                     gboolean                        arg_pressed,
                     gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);
  gboolean handled = FALSE;

  g_signal_emit (engine, g_input_method_engine_signals[ENGINE_SIGNAL_KEY_EVENT],
                 0, arg_keycode, arg_pressed, &handled);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(b)", handled));
  return TRUE;
}

static gboolean
on_handle_focus (_GFreedesktopInputMethodEngine *object,
                 GDBusMethodInvocation          *invocation,
                 gboolean                        focused,
                 gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);

  g_signal_emit (engine, g_input_method_engine_signals[ENGINE_SIGNAL_FOCUS],
                 0, focused);
  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static gboolean
on_handle_reset (_GFreedesktopInputMethodEngine *object,
                 GDBusMethodInvocation          *invocation,
                 gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);

  g_signal_emit (engine, g_input_method_engine_signals[ENGINE_SIGNAL_RESET], 0);
  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static gboolean
on_handle_destroy (_GFreedesktopInputMethodEngine *object,
                   GDBusMethodInvocation          *invocation,
                   gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);

  g_signal_emit (engine, g_input_method_engine_signals[ENGINE_SIGNAL_DESTROY],
                 0);
  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static gboolean
on_handle_set_surrounding_text (_GFreedesktopInputMethodEngine *object,
                                GDBusMethodInvocation          *invocation,
                                const gchar                    *arg_text,
                                guint                           arg_cursor_pos,
                                guint                           arg_anchor_pos,
                                gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);

  g_signal_emit (engine,
                 g_input_method_engine_signals[ENGINE_SIGNAL_SET_SURROUNDING_TEXT],
                 0, arg_text, arg_cursor_pos, arg_anchor_pos);
  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static gboolean
on_handle_set_content_type (_GFreedesktopInputMethodEngine *object,
                            GDBusMethodInvocation          *invocation,
                            guint                           arg_purpose,
                            guint                           arg_hints,
                            gpointer                        user_data)
{
  GInputMethodEngine *engine = G_INPUT_METHOD_ENGINE (user_data);

  g_signal_emit (engine,
                 g_input_method_engine_signals[ENGINE_SIGNAL_SET_CONTENT_TYPE],
                 0, arg_purpose, arg_hints);
  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static void
g_input_method_engine_init (GInputMethodEngine *engine)
{
  engine->priv = g_input_method_engine_get_instance_private (engine);
  engine->priv->skeleton = _g_freedesktop_input_method_engine_skeleton_new ();

  g_signal_connect (engine->priv->skeleton, "handle-key-event",
                    G_CALLBACK (on_handle_key_event),
                    engine);
  g_signal_connect (engine->priv->skeleton, "handle-focus",
                    G_CALLBACK (on_handle_focus),
                    engine);
  g_signal_connect (engine->priv->skeleton, "handle-reset",
                    G_CALLBACK (on_handle_reset),
                    engine);
  g_signal_connect (engine->priv->skeleton, "handle-destroy",
                    G_CALLBACK (on_handle_destroy),
                    engine);
  g_signal_connect (engine->priv->skeleton, "handle-set-surrounding-text",
                    G_CALLBACK (on_handle_set_surrounding_text),
                    engine);
  g_signal_connect (engine->priv->skeleton, "handle-set-content-type",
                    G_CALLBACK (on_handle_set_content_type),
                    engine);
}

static gboolean
g_input_method_engine_export (GInputMethodEngine *engine,
                              GDBusConnection    *connection,
                              const gchar        *object_path,
                              GError            **error)
{
  GDBusInterfaceSkeleton *skeleton
    = G_DBUS_INTERFACE_SKELETON (engine->priv->skeleton);

  return g_dbus_interface_skeleton_export (skeleton,
                                           connection,
                                           object_path,
                                           error);
}

/**
 * g_input_method_engine_commit:
 * @engine: A #GInputMethodEngine.
 * @text: The completed characters entered by user.
 *
 * Notifies the application that a complete input sequence has
 * been entered by the user.
 */
void
g_input_method_engine_commit (GInputMethodEngine *engine,
                              const gchar        *text)
{
  g_signal_emit (engine,
                 g_input_method_engine_signals[ENGINE_SIGNAL_COMMIT],
                 0, text);
}

/**
 * g_input_method_engine_preedit_changed:
 * @engine: A #GInputMethodEngine.
 * @text: The new preedit text.
 * @styling: An array of styling attributes attached to @text.
 * @n_styling: The number of elements in @styling.
 * @cursor_pos: The new caret position.
 *
 * Notifies the application that the preedit text has changed.
 */
void
g_input_method_engine_preedit_changed (GInputMethodEngine  *engine,
                                       const gchar         *text,
                                       GInputMethodStyling *styling,
                                       gsize                n_styling,
                                       gint                 cursor_pos)
{
  g_signal_emit (engine,
                 g_input_method_engine_signals[ENGINE_SIGNAL_PREEDIT_CHANGED],
                 0, text, styling, n_styling, cursor_pos);
}

/**
 * g_input_method_engine_delete_surrounding_text:
 * @engine: A #GInputMethodEngine.
 * @offset: Offset from cursor position in chars; a
 *   negative value means start before the cursor.
 * @nchars: The number of characters to delete.
 *
 * Asks the application to delete characters around the cursor
 * position.
 */
void
g_input_method_engine_delete_surrounding_text (GInputMethodEngine  *engine,
                                               gint                 offset,
                                               guint                nchars)
{
  g_signal_emit (engine,
                 g_input_method_engine_signals[ENGINE_SIGNAL_DELETE_SURROUNDING_TEXT],
                 0, offset, nchars);
}

/**
 * g_input_method_engine_new:
 * @client_id: A client ID.
 *
 * Creates a new #GInputMethodEngine instance.
 *
 * Returns: a #GInputMethodEngine.
 */
GInputMethodEngine *
g_input_method_engine_new (const gchar *client_id)
{
  return g_object_new (G_TYPE_INPUT_METHOD_ENGINE,
                       "client-id", client_id,
                       NULL);
}

typedef struct _Client
{
  GSList *engines;
  GInputMethod *inputmethod;
  guint watcher_id;
} Client;

/**
 * GInputMethodClass:
 * @create_engine: invoked on the input method instance when it is requested
 *     to create a new input method engine.
 *
 * Virtual function table for #GInputMethodEngine.
 */

struct _GInputMethodPrivate
{
  _GFreedesktopInputMethod *skeleton;
  guint owner_id;
  gboolean activating;
  gulong engine_serial;
  GMutex clients_lock;
  GHashTable *clients;
  GDBusConnection *connection;
  gchar *address;
};

enum
{
  PROP_NONE,
  PROP_ADDRESS
};

enum
{
  SIGNAL_CREATE_ENGINE,
  NR_SIGNALS
};

static guint g_input_method_signals[NR_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (GInputMethod, g_input_method, G_TYPE_APPLICATION)

static gboolean      g_input_method_engine_export
                     (GInputMethodEngine *engine,
                      GDBusConnection    *connection,
                      const gchar        *object_path,
                      GError            **error);

static GInputMethodEngine *
g_input_method_real_create_engine (GInputMethod *inputmethod,
                                   const gchar  *client_id)
{
  g_return_val_if_reached (NULL);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  GDBusInterfaceSkeleton *skeleton
    = G_DBUS_INTERFACE_SKELETON (inputmethod->priv->skeleton);
  GError *error = NULL;

  if (!g_dbus_interface_skeleton_export (skeleton,
                                         connection,
                                         "/org/freedesktop/InputMethod",
                                         &error))
    {
      g_application_release (G_APPLICATION (inputmethod));
      g_warning ("Error exporting %s interface: %s", name, error->message);
      g_error_free (error);
    }
  inputmethod->priv->activating = FALSE;
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  GDBusInterfaceSkeleton *skeleton
    = G_DBUS_INTERFACE_SKELETON (inputmethod->priv->skeleton);

  g_dbus_interface_skeleton_unexport (skeleton);
  g_application_release (G_APPLICATION (inputmethod));
  inputmethod->priv->owner_id = 0;
  inputmethod->priv->activating = FALSE;
}

static void
g_input_method_real_activate (GApplication *application)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (application);
  GDBusConnection *connection;

  g_application_hold (application);

  connection = g_application_get_dbus_connection (application);
  if (!connection)
    return;

  /* FIXME: to prevent g_bus_own_name being called successively on
     activation from D-Bus.  */
  if (inputmethod->priv->activating)
    return;

  if (inputmethod->priv->owner_id > 0)
    g_bus_unown_name (inputmethod->priv->owner_id);

  inputmethod->priv->activating = TRUE;
  inputmethod->priv->owner_id
    = g_bus_own_name_on_connection (connection,
                                    "org.freedesktop.InputMethod",
                                    G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT
                                    | G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                    on_name_acquired,
                                    on_name_lost,
                                    g_object_ref (application),
                                    (GDestroyNotify) g_object_unref);
}

static void
g_input_method_real_dispose (GObject *object)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (object);

  g_clear_object (&inputmethod->priv->connection);
  g_clear_pointer (&inputmethod->priv->address, g_free);
  g_clear_pointer (&inputmethod->priv->clients, g_hash_table_unref);

  G_OBJECT_CLASS (g_input_method_parent_class)->dispose (object);
}

static void
g_input_method_real_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      inputmethod->priv->address = g_value_dup_string (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
create_engine_accumulator (GSignalInvocationHint *ihint,
                           GValue                *return_accu,
                           const GValue          *handler_return,
                           gpointer               data)
{
  GInputMethodEngine *engine;

  engine = g_value_get_object (handler_return);
  if (engine)
    {
      g_value_set_object (return_accu, engine);
      return FALSE;
    }

  return TRUE;
}

static void
g_input_method_class_init (GInputMethodClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  klass->create_engine = g_input_method_real_create_engine;
  application_class->activate = g_input_method_real_activate;
  object_class->set_property = g_input_method_real_set_property;
  object_class->dispose = g_input_method_real_dispose;

  g_object_class_install_property (object_class, PROP_ADDRESS,
    g_param_spec_string ("address",
                         P_("Address"),
                         P_("The D-Bus address to which engines are exported"),
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * GInputMethod::create-engine:
   *
   * The ::create-engine signal is emitted on the input method
   * instance when it is requested to create a new input method engine.
   */
  g_input_method_signals[SIGNAL_CREATE_ENGINE] =
    g_signal_new ("create-engine", G_TYPE_INPUT_METHOD, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GInputMethodClass, create_engine),
                  create_engine_accumulator,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_INPUT_METHOD_ENGINE,
                  1,
                  G_TYPE_STRING);
}

static void
on_engine_destroy (GInputMethodEngine *engine,
                   gpointer            user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  Client *client;

  g_mutex_lock (&inputmethod->priv->clients_lock);
  client = g_hash_table_lookup (inputmethod->priv->clients,
                                engine->priv->client_id);
  if (client)
    {
      GSList *link = g_slist_find (client->engines, engine);
      client->engines = g_slist_delete_link (client->engines, link);
      g_application_release (G_APPLICATION (inputmethod));
    }
  g_mutex_unlock (&inputmethod->priv->clients_lock);
  g_object_unref (engine);
}

static gboolean
on_handle_get_address (_GFreedesktopInputMethod *object,
                       GDBusMethodInvocation    *invocation,
                       gpointer                  user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  GVariant *parameters;

  parameters = g_variant_new ("(s)",
                              inputmethod->priv->address == NULL
                              ? "" : inputmethod->priv->address);
  g_dbus_method_invocation_return_value (invocation, parameters);

  return TRUE;
}

static void
on_client_name_vanished (GDBusConnection *connection,
                         const gchar     *name,
                         gpointer         user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  g_hash_table_remove (inputmethod->priv->clients, name);
}

static gboolean
on_handle_create_engine (_GFreedesktopInputMethod *object,
                         GDBusMethodInvocation    *invocation,
                         GVariant                 *arg_platform_data,
                         gpointer                  user_data)
{
  GInputMethod *inputmethod = G_INPUT_METHOD (user_data);
  GInputMethodEngine *engine;
  const gchar *sender;
  const gchar *object_path;
  gchar *engine_object_path;
  GVariant *parameters;
  GError *error;
  Client *client;

  G_APPLICATION_GET_CLASS (inputmethod)
    ->before_emit (G_APPLICATION (inputmethod), arg_platform_data);

  sender = g_dbus_method_invocation_get_sender (invocation);
  g_signal_emit (inputmethod, g_input_method_signals[SIGNAL_CREATE_ENGINE],
                 0, sender, &engine);

  G_APPLICATION_GET_CLASS (inputmethod)
    ->after_emit (G_APPLICATION (inputmethod), arg_platform_data);

  if (!engine)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Cannot create engine");
      return TRUE;
    }

  object_path
    = g_application_get_dbus_object_path (G_APPLICATION (inputmethod));

  if (!inputmethod->priv->connection)
    {
      if (inputmethod->priv->address == NULL
          || *inputmethod->priv->address == '\0')
        {
          if (!g_application_get_is_registered (G_APPLICATION (inputmethod)))
            {
              g_object_unref (engine);
              g_dbus_method_invocation_return_error (invocation,
                                                     G_DBUS_ERROR,
                                                     G_DBUS_ERROR_FAILED,
                                                     "Application has not yet been registered");
              return TRUE;
            }

          inputmethod->priv->connection
            = g_application_get_dbus_connection (G_APPLICATION (inputmethod));
          if (!inputmethod->priv->connection)
            {
              g_object_unref (engine);
              g_dbus_method_invocation_return_error (invocation,
                                                     G_DBUS_ERROR,
                                                     G_DBUS_ERROR_FAILED,
                                                     "Application has no D-Bus connection");
              return TRUE;
            }

          g_object_ref (inputmethod->priv->connection);
        }
      else
        {
          error = NULL;
          inputmethod->priv->connection
            = g_dbus_connection_new_for_address_sync (inputmethod->priv->address,
                                                      G_DBUS_CONNECTION_FLAGS_NONE,
                                                      NULL,
                                                      NULL,
                                                      &error);
          if (!inputmethod->priv->connection)
            {
              g_object_unref (engine);
              g_dbus_method_invocation_return_gerror (invocation, error);
              return TRUE;
            }
        }
    }

  engine_object_path = g_strdup_printf ("%s/Engine_%lu",
                                        object_path,
                                        inputmethod->priv->engine_serial++);

  error = NULL;
  if (!g_input_method_engine_export (engine,
                                     inputmethod->priv->connection,
                                     engine_object_path,
                                     &error))
    {
      g_object_unref (engine);
      g_free (engine_object_path);
      g_dbus_method_invocation_return_gerror (invocation, error);
      return TRUE;
    }

  parameters = g_variant_new ("(o)", engine_object_path);
  g_free (engine_object_path);
  g_dbus_method_invocation_return_value (invocation, parameters);

  g_signal_connect (engine, "destroy", G_CALLBACK (on_engine_destroy),
                    inputmethod);
  g_application_hold (G_APPLICATION (inputmethod));

  g_mutex_lock (&inputmethod->priv->clients_lock);
  client = g_hash_table_lookup (inputmethod->priv->clients, sender);
  if (!client)
    {
      client = g_slice_new0 (Client);
      client->inputmethod = inputmethod;
      client->watcher_id
        = g_bus_watch_name_on_connection (g_dbus_method_invocation_get_connection (invocation),
                                          sender,
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          NULL,
                                          on_client_name_vanished,
                                          inputmethod,
                                          NULL);
      g_hash_table_insert (inputmethod->priv->clients,
                           g_strdup (sender), client);
    }
  client->engines = g_slist_prepend (client->engines, engine);
  g_mutex_unlock (&inputmethod->priv->clients_lock);

  return TRUE;
}

static void
client_destroy (gpointer data)
{
  Client *client = data;
  GSList *l;

  g_bus_unwatch_name (client->watcher_id);

  for (l = client->engines; l; l = l->next)
    {
      GInputMethodEngine *engine = l->data;

      g_object_unref (engine);
      g_application_release (G_APPLICATION (client->inputmethod));
    }

  g_slist_free (client->engines);
  g_slice_free (Client, client);
}

static void
g_input_method_init (GInputMethod *inputmethod)
{
  inputmethod->priv = g_input_method_get_instance_private (inputmethod);
  inputmethod->priv->skeleton = _g_freedesktop_input_method_skeleton_new ();
  inputmethod->priv->clients
    = g_hash_table_new_full (g_str_hash,
                             g_str_equal,
                             g_free,
                             client_destroy);
  g_mutex_init (&inputmethod->priv->clients_lock);

  g_signal_connect (inputmethod->priv->skeleton, "handle-get-address",
                    G_CALLBACK (on_handle_get_address),
                    inputmethod);
  g_signal_connect (inputmethod->priv->skeleton, "handle-create-engine",
                    G_CALLBACK (on_handle_create_engine),
                    inputmethod);
}

/**
 * g_input_method_new:
 * @application_id: The application ID.
 * @flags: The application flags.
 * @address: The D-Bus address to which engines are exported on.
 *
 * Creates a new #GInputMethod instance.  If @address is %NULL,
 * engines are exported on the same D-Bus session as #GInputMethod itself.
 *
 * Returns: a #GInputMethod.
 */
GInputMethod *
g_input_method_new (const gchar      *application_id,
                    GApplicationFlags flags,
                    const gchar      *address)
{
  return g_object_new (G_TYPE_INPUT_METHOD,
                       "application-id", application_id,
                       "flags", flags,
                       "address", address,
                       NULL);
}
