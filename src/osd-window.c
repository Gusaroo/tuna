/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "phosh-osd-window"

#include "config.h"

#include "osd-window.h"

/**
 * SECTION:osd-window
 * @short_description: A OSD Window
 * @Title: PhoshOsdWindow
 *
 * The #PhoshOsdWindow displays contents fed via the
 * OSD (on screen display) DBus interface.
 */

enum {
  PROP_0,
  PROP_CONNECTOR,
  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_LEVEL,
  PROP_MAX_LEVEL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

typedef struct _PhoshOsdWindow {
  PhoshSystemModal parent;

  char            *connector;
  char            *label;
  char            *icon_name;
  gdouble          level;
  gdouble          max_level;

  GtkWidget       *lbl;
  GtkWidget       *icon;
  GtkWidget       *bar;
  GtkGesture      *click_gesture;
} PhoshOsdWindow;


G_DEFINE_TYPE (PhoshOsdWindow, phosh_osd_window, PHOSH_TYPE_SYSTEM_MODAL)

static void
phosh_osd_window_set_property (GObject      *obj,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PhoshOsdWindow *self = PHOSH_OSD_WINDOW (obj);

  switch (prop_id) {
  case PROP_CONNECTOR:
    g_free (self->connector);
    self->connector = g_value_dup_string (value);
    break;
  case PROP_LABEL:
    g_free (self->label);
    self->label = g_value_dup_string (value);
    gtk_label_set_label (GTK_LABEL (self->lbl), self->label);
    break;
  case PROP_ICON_NAME:
    g_free (self->icon_name);
    self->icon_name = g_value_dup_string (value);
    gtk_image_set_from_icon_name (GTK_IMAGE (self->icon), self->icon_name, GTK_ICON_SIZE_INVALID);
    break;
  case PROP_LEVEL:
    self->level = g_value_get_double (value);
    gtk_level_bar_set_value (GTK_LEVEL_BAR (self->bar), self->level);
    break;
  case PROP_MAX_LEVEL:
    self->max_level = g_value_get_double (value);
    gtk_level_bar_set_max_value (GTK_LEVEL_BAR (self->bar), self->max_level);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}


static void
phosh_osd_window_get_property (GObject    *obj,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PhoshOsdWindow *self = PHOSH_OSD_WINDOW (obj);

  switch (prop_id) {
  case PROP_CONNECTOR:
    g_value_set_string (value, self->connector ? self->connector : "");
    break;
  case PROP_LABEL:
    g_value_set_string (value, self->label ? self->label : "");
    break;
  case PROP_ICON_NAME:
    g_value_set_string (value, self->icon_name ? self->icon_name : "");
    break;
  case PROP_LEVEL:
    g_value_set_double (value, self->level);
    break;
  case PROP_MAX_LEVEL:
    g_value_set_double (value, self->max_level);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}


static void
on_button_released (PhoshOsdWindow *self)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}


static void
phosh_osd_window_finalize (GObject *obj)
{
  PhoshOsdWindow *self = PHOSH_OSD_WINDOW (obj);

  g_free (self->connector);
  g_free (self->label);
  g_free (self->icon_name);

  G_OBJECT_CLASS (phosh_osd_window_parent_class)->finalize (obj);
}


static void
phosh_osd_window_class_init (PhoshOsdWindowClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = phosh_osd_window_get_property;
  object_class->set_property = phosh_osd_window_set_property;
  object_class->finalize = phosh_osd_window_finalize;

  /* TODO: currently unused */
  props[PROP_CONNECTOR] =
    g_param_spec_string ("connector",
                         "Connector",
                         "Connector to use for osd display",
                         NULL,
                         G_PARAM_READWRITE);

  props[PROP_LABEL] =
    g_param_spec_string ("label",
                         "Label",
                         "Label to show on osd",
                         NULL,
                         G_PARAM_READWRITE);

  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Name of icon to use on osd",
                         NULL,
                         G_PARAM_READWRITE);

  props[PROP_LEVEL] =
    g_param_spec_double ("level",
                         "Level",
                         "Level of bar to display on osd",
                         0.0,
                         G_MAXDOUBLE,
                         0.0,
                         G_PARAM_READWRITE);

  props[PROP_MAX_LEVEL] =
    g_param_spec_double ("max-level",
                         "Maximum Level",
                         "Maximum level of bar to display on osd",
                         0.0,
                         G_MAXDOUBLE,
                         0.0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/phosh/ui/osd-window.ui");
  gtk_widget_class_bind_template_child (widget_class, PhoshOsdWindow, lbl);
  gtk_widget_class_bind_template_child (widget_class, PhoshOsdWindow, icon);
  gtk_widget_class_bind_template_child (widget_class, PhoshOsdWindow, bar);
  gtk_widget_class_bind_template_child (widget_class, PhoshOsdWindow, click_gesture);
  gtk_widget_class_bind_template_callback (widget_class, on_button_released);

  gtk_widget_class_set_css_name (widget_class, "phosh-osd-window");
}


static void
phosh_osd_window_init (PhoshOsdWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_RELEASE_MASK);
}


GtkWidget *
phosh_osd_window_new (char  *connector,
                      char  *label,
                      char  *icon_name,
                      double level,
                      double max_level)
{
  return g_object_new (PHOSH_TYPE_OSD_WINDOW,
                       "connector", connector,
                       "label",label,
                       "icon-name", icon_name,
                       "level", level,
                       "max-level", max_level,
                       NULL);
}
