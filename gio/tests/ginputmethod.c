#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gdbus-tests.h"
#include "gdbus-sessionbus.h"

typedef struct
{
  GMainLoop *loop;
  gchar *object_path;
  gchar *name;
  GDBusProxy *proxy;
  gboolean expect_key_event_handled;
} ClientData;

static gboolean activated;

static gboolean
stop_loop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
key_event_ready (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  ClientData *data = user_data;
  GVariant *value;
  GError *error = NULL;
  gboolean handled;

  value = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  g_assert (value);
  g_assert_no_error (error);
  g_variant_get (value, "(b)", &handled);
  g_assert_cmpint (handled, ==, data->expect_key_event_handled);
  g_main_loop_quit (data->loop);
}

static void
destroy_ready (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  ClientData *data = user_data;
  GVariant *value;
  GError *error = NULL;

  value = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  g_assert (value);
  g_assert_no_error (error);
  g_main_loop_quit (data->loop);
}

static void
test_engine_client (ClientData *data)
{
  data->expect_key_event_handled = FALSE;
  g_dbus_proxy_call (data->proxy,
                     "KeyEvent",
                     g_variant_new ("(ub)", 38, TRUE),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     key_event_ready,
                     data);
  g_main_loop_run (data->loop);
  data->expect_key_event_handled = TRUE;
  g_dbus_proxy_call (data->proxy,
                     "KeyEvent",
                     g_variant_new ("(ub)", 24, TRUE),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     key_event_ready,
                     data);
  g_main_loop_run (data->loop);

  /* FIXME: add more tests to check incoming signals.  */

  g_dbus_proxy_call (data->proxy,
                     "Destroy",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     destroy_ready,
                     data);
  g_main_loop_run (data->loop);
}

static void
create_engine_proxy_ready (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GError *error = NULL;
  ClientData *data = user_data;

  data->proxy = g_dbus_proxy_new_finish (res, &error);
  g_assert (data->proxy != NULL);
  g_assert_no_error (error);

  g_main_loop_quit (data->loop);
}

static void
create_engine_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  ClientData *data = user_data;
  GError *error = NULL;
  GVariant *value;
  gchar *p;

  value = g_dbus_connection_call_finish (connection, res, &error);
  g_assert (value != NULL);
  g_assert_no_error (error);
  g_clear_error (&error);

  g_variant_get (value, "(o)", &data->object_path);
  g_variant_unref (value);

  g_assert (g_str_has_prefix (data->object_path,
                              "/org/gtk/UnimportantInputMethod/"));

  data->name = g_strdup (data->object_path + 1);
  p = strrchr (data->name, '/');
  g_assert (p);
  *p = '\0';
  for (p = data->name; *p != '\0'; p++)
    if (*p == '/')
      *p = '.';

  /* FIXME: check "Address" D-Bus property.  */
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    data->name,
                    data->object_path,
                    "org.freedesktop.InputMethod.Engine",
                    NULL,
                    create_engine_proxy_ready,
                    data);
}

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_dbus_connection_call (connection,
                          name,
                          "/org/freedesktop/InputMethod",
                          "org.freedesktop.InputMethod",
                          "CreateEngine",
                          g_variant_new ("(a{sv})", &builder),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          create_engine_ready,
                          user_data);
}

static gpointer
test_create_engine_client_thread_func (gpointer user_data)
{
  GApplication *app = G_APPLICATION (user_data);
  ClientData data;
  guint watcher_id;

  memset (&data, 0, sizeof (data));
  data.loop = g_main_loop_new (NULL, FALSE);

  watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                 "org.freedesktop.InputMethod",
                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
                                 on_name_appeared,
                                 NULL,
                                 &data,
                                 NULL);

  g_timeout_add (100, stop_loop, data.loop);
  g_main_loop_run (data.loop);
  g_assert (data.proxy);

  test_engine_client (&data);

  g_bus_unwatch_name (watcher_id);

  /* FIXME: check if owning "org.freedestkop.InputMethod" decreases
     the use count of the service.  */
  g_application_release (app);

  return NULL;
}

static void
on_activate (GApplication *app)
{
  activated = TRUE;

  g_assert (g_application_get_dbus_connection (app) != NULL);
  g_assert (g_application_get_dbus_object_path (app) != NULL);
}

static gboolean
on_key_event (GInputMethodEngine *engine,
              guint               keycode,
              gboolean            pressed,
              gpointer            user_data)
{
  g_input_method_engine_commit (engine, "Hello");

  return keycode == 24;
}

static GInputMethodEngine *
on_create_engine (GInputMethod *inputmethod,
                  const gchar  *client_id)
{
  GInputMethodEngine *engine;

  engine = g_input_method_engine_new (client_id);
  g_signal_connect (engine, "key-event", G_CALLBACK (on_key_event), NULL);
  return engine;
}

static void
test_basic (void)
{
  GDBusConnection *c;
  GInputMethod *inputmethod;
  GThread *client_thread;

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  inputmethod = g_input_method_new ("org.gtk.UnimportantInputMethod",
                                    G_APPLICATION_FLAGS_NONE,
                                    NULL);
  activated = FALSE;
  g_signal_connect (inputmethod, "activate", G_CALLBACK (on_activate), NULL);
  g_signal_connect (inputmethod, "create-engine",
                    G_CALLBACK (on_create_engine), NULL);

  /* run the D-Bus client in a thread */
  client_thread = g_thread_new ("ginputmethod-client-thread",
                                test_create_engine_client_thread_func,
                                inputmethod);

  g_application_run (G_APPLICATION (inputmethod), 0, NULL);
  g_thread_join (client_thread);
  g_object_unref (inputmethod);
  g_object_unref (c);

  g_assert (activated);

  session_bus_down ();
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_dbus_unset ();

  g_test_add_func ("/ginputmethod/basic", test_basic);

  return g_test_run ();
}
