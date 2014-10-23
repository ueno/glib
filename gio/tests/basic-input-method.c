#include <gio/gio.h>
#include <string.h>

static gboolean
key_event (GInputMethodEngine *engine,
           guint               keycode,
           gboolean            pressed,
           gpointer            user_data)
{
  if (keycode == 38 && pressed)            /* a */
    {
      g_input_method_engine_commit (engine, "Hello!");
      return TRUE;
    }

  return FALSE;
}

static GInputMethodEngine *
create_engine (GInputMethod *inputmethod,
               const gchar  *client_id,
               gpointer      user_data)
{
  GInputMethodEngine *engine = g_input_method_engine_new (client_id);

  g_signal_connect (engine, "key-event", G_CALLBACK (key_event), NULL);

  return engine;
}

int
main (int argc, char **argv)
{
  GInputMethod *inputmethod;
  int status;

  inputmethod = g_input_method_new ("org.gtk.TestInputMethod",
                                    G_APPLICATION_FLAGS_NONE,
                                    NULL);
  g_signal_connect (inputmethod, "create-engine", G_CALLBACK (create_engine),
                    NULL);
#ifdef STANDALONE
  g_application_set_inactivity_timeout (G_APPLICATION (inputmethod), 10000);
#else
  g_application_set_inactivity_timeout (G_APPLICATION (inputmethod), 1000);
#endif

  status = g_application_run (G_APPLICATION (inputmethod), argc - 1, argv + 1);

  g_object_unref (inputmethod);

  g_print ("exit status: %d\n", status);

  return 0;
}
