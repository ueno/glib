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

#ifndef __G_INPUT_METHOD_H__
#define __G_INPUT_METHOD_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>
#include <gio/gapplication.h>

G_BEGIN_DECLS

typedef struct _GInputMethodStyling GInputMethodStyling;

/**
 * GInputMethodStyling:
 * @start: The starting character position.
 * @end: The ending character position (exclusive).
 * @type: The #GInputMethodStylingType.
 *
 * Styling attribute applied to strings.
 */
struct _GInputMethodStyling
{
  guint start;
  guint end;
  GInputMethodStylingType type;
};

#define G_TYPE_INPUT_METHOD_ENGINE (g_input_method_engine_get_type ())
#define G_INPUT_METHOD_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_INPUT_METHOD_ENGINE, GInputMethodEngine))
#define G_INPUT_METHOD_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_INPUT_METHOD_ENGINE, GInputMethodEngineClass))
#define G_IS_INPUT_METHOD_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_INPUT_METHOD_ENGINE))
#define G_IS_INPUT_METHOD_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_INPUT_METHOD_ENGINE))
#define G_INPUT_METHOD_ENGINE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_INPUT_METHOD_ENGINE, GInputMethodEngineClass))

typedef struct _GInputMethodEnginePrivate GInputMethodEnginePrivate;
typedef struct _GInputMethodEngineClass GInputMethodEngineClass;

/**
 * GInputMethodEngine:
 *
 * D-Bus service that implements an input method engine.
 */
struct _GInputMethodEngine
{
  /*< private >*/
  GObject parent_instance;

  GInputMethodEnginePrivate *priv;
};

struct _GInputMethodEngineClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* signals handled by the engine */
  gboolean (* key_event)            (GInputMethodEngine  *engine,
                                     guint                keycode,
                                     gboolean             pressed);
  void     (* focus)                (GInputMethodEngine  *engine,
                                     gboolean             focused);
  void     (* reset)                (GInputMethodEngine  *engine);
  void     (* destroy)              (GInputMethodEngine  *engine);

  void     (* set_surrounding_text) (GInputMethodEngine  *engine,
                                     const gchar         *text,
                                     guint                cursor_pos,
                                     guint                anchor_pos);

  void     (* set_content_type)     (GInputMethodEngine  *engine,
                                     GInputMethodPurpose  purpose,
                                     GInputMethodHints    hints);

  /* signals emitted by the engine */
  void     (* commit)               (GInputMethodEngine  *engine,
                                     const gchar         *text);
  void     (* preedit_changed)      (GInputMethodEngine  *engine,
                                     const gchar         *text,
                                     GInputMethodStyling *styling,
                                     gsize                n_styling,
                                     gint                 cursor_pos);
  void     (* delete_surrounding_text)
                                    (GInputMethodEngine  *engine,
                                     gint                 offset,
                                     guint                nchars);
};

GLIB_AVAILABLE_IN_ALL
GType               g_input_method_engine_get_type
                                              (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_ALL
GInputMethodEngine *g_input_method_engine_new (const gchar         *client_id);

GLIB_AVAILABLE_IN_ALL
void                g_input_method_engine_commit
                                              (GInputMethodEngine  *engine,
                                               const gchar         *text);
GLIB_AVAILABLE_IN_ALL
void                g_input_method_engine_preedit_changed
                                              (GInputMethodEngine  *engine,
                                               const gchar         *text,
                                               GInputMethodStyling *styling,
                                               gsize                n_styling,
                                               gint                 cursor_pos);
GLIB_AVAILABLE_IN_ALL
void                g_input_method_engine_delete_surrounding_text
                                              (GInputMethodEngine  *engine,
                                               gint                 offset,
                                               guint                nchars);

#define G_TYPE_INPUT_METHOD (g_input_method_get_type ())
#define G_INPUT_METHOD(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_INPUT_METHOD, GInputMethod))
#define G_INPUT_METHOD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_INPUT_METHOD, GInputMethodClass))
#define G_IS_INPUT_METHOD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_INPUT_METHOD))
#define G_IS_INPUT_METHOD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_INPUT_METHOD))
#define G_INPUT_METHOD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_INPUT_METHOD, GInputMethodClass))

typedef struct _GInputMethodPrivate GInputMethodPrivate;
typedef struct _GInputMethodClass GInputMethodClass;

/**
 * GInputMethod:
 *
 * Special #GApplication that launches #GInputMethodEngine on the D-Bus.
 */
struct _GInputMethod
{
  /*< private >*/
  GApplication parent_instance;

  GInputMethodPrivate *priv;
};

struct _GInputMethodClass
{
  /*< private >*/
  GApplicationClass parent_class;

  /*< public >*/
  /* signals */
  GInputMethodEngine * (* create_engine)  (GInputMethod *inputmethod,
                                           const gchar  *client_id);
};

GLIB_AVAILABLE_IN_ALL
GType         g_input_method_get_type (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_ALL
GInputMethod *g_input_method_new      (const gchar      *application_id,
                                       GApplicationFlags flags,
                                       const gchar      *address);

G_END_DECLS

#endif  /* __G_INPUT_METHOD_H__ */
