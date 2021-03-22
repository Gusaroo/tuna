/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Julian Sparber <julian.sparber@puri.sm>
 */

#define G_LOG_DOMAIN "phosh-status-icon"

#include "config.h"

#include "status-icon.h"

/**
 * SECTION:status-icon
 * @short_description: Base clase for different status icons e.g in the top bar
 * @Title: PhoshStatusIcon
 */

enum {
  PHOSH_STATUS_ICON_PROP_0,
  PHOSH_STATUS_ICON_PROP_ICON_NAME,
  PHOSH_STATUS_ICON_PROP_ICON_SIZE,
  PHOSH_STATUS_ICON_PROP_EXTRA_WIDGET,
  PHOSH_STATUS_ICON_PROP_INFO,
  PHOSH_STATUS_ICON_PROP_LAST_PROP
};
static GParamSpec *props[PHOSH_STATUS_ICON_PROP_LAST_PROP];

typedef struct
{
  GtkWidget *image;
  GtkWidget *extra_widget;
  GtkIconSize icon_size;
  char *info;
} PhoshStatusIconPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PhoshStatusIcon, phosh_status_icon, GTK_TYPE_BIN);


static void
phosh_status_icon_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  PhoshStatusIcon *self = PHOSH_STATUS_ICON (object);

  switch (property_id) {
  case PHOSH_STATUS_ICON_PROP_ICON_NAME:
    phosh_status_icon_set_icon_name (self, g_value_get_string (value));
    break;
  case PHOSH_STATUS_ICON_PROP_ICON_SIZE:
    phosh_status_icon_set_icon_size (self, g_value_get_enum (value));
    break;
  case PHOSH_STATUS_ICON_PROP_EXTRA_WIDGET:
    phosh_status_icon_set_extra_widget (self, g_value_get_object (value));
    break;
  case PHOSH_STATUS_ICON_PROP_INFO:
    phosh_status_icon_set_info (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
phosh_status_icon_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  PhoshStatusIcon *self = PHOSH_STATUS_ICON (object);

  switch (property_id) {
  case PHOSH_STATUS_ICON_PROP_ICON_NAME:
    g_value_set_string (value, phosh_status_icon_get_icon_name (self));
    break;
  case PHOSH_STATUS_ICON_PROP_ICON_SIZE:
    g_value_set_enum (value, phosh_status_icon_get_icon_size (self));
    break;
  case PHOSH_STATUS_ICON_PROP_EXTRA_WIDGET:
    g_value_set_object (value, phosh_status_icon_get_extra_widget (self));
    break;
  case PHOSH_STATUS_ICON_PROP_INFO:
    g_value_set_string (value, phosh_status_icon_get_info (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
phosh_status_icon_finalize (GObject *gobject)
{
  PhoshStatusIconPrivate *priv = phosh_status_icon_get_instance_private (PHOSH_STATUS_ICON (gobject));

  g_clear_pointer (&priv->info, g_free);

  G_OBJECT_CLASS (phosh_status_icon_parent_class)->finalize (gobject);
}


static void
phosh_status_icon_class_init (PhoshStatusIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = phosh_status_icon_set_property;
  object_class->get_property = phosh_status_icon_get_property;
  object_class->finalize = phosh_status_icon_finalize;

  props[PHOSH_STATUS_ICON_PROP_ICON_NAME] =
   g_param_spec_string ("icon-name",
                        "icon name",
                        "The icon name",
                        NULL,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PHOSH_STATUS_ICON_PROP_ICON_SIZE] =
   g_param_spec_enum ("icon-size",
		      "icon size",
		      "The icon size",
		      GTK_TYPE_ICON_SIZE,
		      GTK_ICON_SIZE_LARGE_TOOLBAR,
		      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PHOSH_STATUS_ICON_PROP_EXTRA_WIDGET] =
   g_param_spec_object ("extra_widget",
			"extra widget",
			"An additional widget",
			GTK_TYPE_WIDGET,
			G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PHOSH_STATUS_ICON_PROP_INFO] =
   g_param_spec_string ("info",
                        "info",
                        "Additional state information",
                        NULL,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PHOSH_STATUS_ICON_PROP_LAST_PROP, props);
}


static void
phosh_status_icon_init (PhoshStatusIcon *self)
{
  PhoshStatusIconPrivate *priv = phosh_status_icon_get_instance_private (self);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  gtk_widget_set_visible (box, TRUE);

  priv->icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
  priv->image = gtk_image_new();
  gtk_widget_set_visible (priv->image, TRUE);

  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->image));

  if (priv->extra_widget) {
    gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->extra_widget));
    gtk_widget_set_visible (priv->extra_widget, TRUE);
  }

  gtk_container_add (GTK_CONTAINER (self), box);
}


GtkWidget *
phosh_status_icon_new (void)
{
  return g_object_new (PHOSH_TYPE_STATUS_ICON, NULL);
}


void
phosh_status_icon_set_icon_name (PhoshStatusIcon *self, const char *icon_name)
{
  PhoshStatusIconPrivate *priv;
  g_autofree char *old_icon_name = NULL;

  g_return_if_fail (PHOSH_IS_STATUS_ICON (self));

  priv = phosh_status_icon_get_instance_private (self);

  old_icon_name = phosh_status_icon_get_icon_name (self);
  if (!g_strcmp0 (old_icon_name, icon_name))
    return;

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				icon_name,
				phosh_status_icon_get_icon_size (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_STATUS_ICON_PROP_ICON_NAME]);
}


char *
phosh_status_icon_get_icon_name (PhoshStatusIcon *self)
{
  PhoshStatusIconPrivate *priv;
  char *icon_name;

  g_return_val_if_fail (PHOSH_IS_STATUS_ICON (self), 0);

  priv = phosh_status_icon_get_instance_private (self);

  g_object_get (priv->image, "icon-name", &icon_name, NULL);

  return icon_name;
}


void
phosh_status_icon_set_icon_size (PhoshStatusIcon *self, GtkIconSize size)
{
  PhoshStatusIconPrivate *priv;
  g_return_if_fail (PHOSH_IS_STATUS_ICON (self));

  priv = phosh_status_icon_get_instance_private (self);

  if (priv->icon_size == size)
    return;

  priv->icon_size = size;
  g_object_set (priv->image, "icon-size", size, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_STATUS_ICON_PROP_ICON_SIZE]);
}


GtkIconSize
phosh_status_icon_get_icon_size (PhoshStatusIcon *self)
{
  PhoshStatusIconPrivate *priv;

  g_return_val_if_fail (PHOSH_IS_STATUS_ICON (self), 0);

  priv = phosh_status_icon_get_instance_private (self);

  return priv->icon_size;
}


void
phosh_status_icon_set_extra_widget (PhoshStatusIcon *self, GtkWidget *widget)
{
  PhoshStatusIconPrivate *priv;
  GtkWidget *box;
  g_return_if_fail (PHOSH_IS_STATUS_ICON (self));

  priv = phosh_status_icon_get_instance_private (self);
  box = gtk_bin_get_child (GTK_BIN (self));

  if (priv->extra_widget == widget)
    return;

  if (priv->extra_widget != NULL)
    gtk_container_remove (GTK_CONTAINER (box), priv->extra_widget);

  if (widget != NULL)
    gtk_container_add (GTK_CONTAINER (box), widget);

  priv->extra_widget = widget;

  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_STATUS_ICON_PROP_EXTRA_WIDGET]);
}


GtkWidget *
phosh_status_icon_get_extra_widget (PhoshStatusIcon *self)
{
  PhoshStatusIconPrivate *priv;

  g_return_val_if_fail (PHOSH_IS_STATUS_ICON (self), 0);

  priv = phosh_status_icon_get_instance_private (self);

  return priv->extra_widget;
}


char *
phosh_status_icon_get_info (PhoshStatusIcon *self)
{
  PhoshStatusIconPrivate *priv;

  g_return_val_if_fail (PHOSH_IS_STATUS_ICON (self), 0);

  priv = phosh_status_icon_get_instance_private (self);

  return priv->info;
}


void
phosh_status_icon_set_info (PhoshStatusIcon *self, const char *info)
{
  PhoshStatusIconPrivate *priv;
  g_return_if_fail (PHOSH_IS_STATUS_ICON (self));

  priv = phosh_status_icon_get_instance_private (self);

  if (g_strcmp0 (priv->info, info) == 0)
    return;

  g_clear_pointer (&priv->info, g_free);
  priv->info = g_strdup (info);

  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_STATUS_ICON_PROP_INFO]);
}

