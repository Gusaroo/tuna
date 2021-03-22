/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Julian Sparber <julian.sparber@puri.sm>
 */

#define G_LOG_DOMAIN "phosh-quick-setting"

#include "config.h"

#include "status-icon.h"
#include "quick-setting.h"

/**
 * SECTION:quick-setting
 * @short_description: A quick setting for the notification drawer
 * @Title: PhoshQuickSetting
 *
 * The #PhoshQuickSetting is a widget which is meant to be placed inside the top drawer.
 * It contains a #GtkLabel and accepts one #PhoshStatusIcon as a child. The info property
 * of the #PhoshStatusIcon is bind to the #GtkLabel.
 * A #PhoshQuickSetting has two signals long_press and clicked, where the first is emited
 * when the user performs a long press, the second signal is a normal single click.
 */

enum {
  LONG_PRESSED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum {
  PHOSH_QUICK_SETTING_PROP_0,
  PHOSH_QUICK_SETTING_PROP_STATUS_ICON,
  PHOSH_QUICK_SETTING_PROP_LAST_PROP
};
static GParamSpec *props[PHOSH_QUICK_SETTING_PROP_LAST_PROP];

typedef struct
{
  GtkWidget *box;
  PhoshStatusIcon *status_icon;
  GtkWidget *label;
  GBinding *label_binding;
  GtkGesture *long_press;
} PhoshQuickSettingPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PhoshQuickSetting, phosh_quick_setting, GTK_TYPE_BUTTON);


static void
phosh_quick_setting_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  PhoshQuickSetting *self = PHOSH_QUICK_SETTING (object);

  switch (property_id) {
  case PHOSH_QUICK_SETTING_PROP_STATUS_ICON:
    g_value_set_object (value, phosh_quick_setting_get_status_icon (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
status_icon_destroy_cb (PhoshQuickSetting *self, GtkWidget *widget)
{
  PhoshQuickSettingPrivate *priv = phosh_quick_setting_get_instance_private (self);

  priv->status_icon = NULL;
  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_QUICK_SETTING_PROP_STATUS_ICON]);
}


static void
phosh_quick_setting_add (GtkContainer *container, GtkWidget *child)
{
  PhoshQuickSetting *self = PHOSH_QUICK_SETTING (container);
  PhoshQuickSettingPrivate *priv = phosh_quick_setting_get_instance_private (self);

  /* If the button doesn't have a child, add the new child directly to it */
  if (gtk_bin_get_child (GTK_BIN (container)) == NULL) {
    GTK_CONTAINER_CLASS (phosh_quick_setting_parent_class)->add (GTK_CONTAINER (self), child);
    return;
  }

  g_return_if_fail (PHOSH_IS_STATUS_ICON (child));

  if (priv->status_icon != NULL) {
    g_warning ("Attempting to add more then one status icon, "
               "but the PhoshQuickSetting can only contain one StatusIcon at a time");
    return;
  }

  priv->status_icon = PHOSH_STATUS_ICON (child);
  gtk_widget_set_halign (GTK_WIDGET (child), GTK_ALIGN_CENTER);
  priv->label_binding = g_object_bind_property (child,
                                               "info",
                                               priv->label,
                                               "label",
                                               G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (child, "destroy", G_CALLBACK (status_icon_destroy_cb), self);

  gtk_box_pack_start (GTK_BOX (priv->box), child, 0, 0, 0);
  g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_QUICK_SETTING_PROP_STATUS_ICON]);
}

static void
phosh_quick_setting_remove (GtkContainer *container, GtkWidget *child)
{
  PhoshQuickSetting *self = PHOSH_QUICK_SETTING (container);
  PhoshQuickSettingPrivate *priv = phosh_quick_setting_get_instance_private (self);

  /* If the child is a StatusIcon remove it from box. Else just remove the child
     of the button */
  if (PHOSH_IS_STATUS_ICON (priv->status_icon) && GTK_WIDGET (priv->status_icon) == child) {
    g_clear_pointer (&priv->label_binding, g_binding_unbind);
    if (priv->status_icon != NULL)
      g_signal_handlers_disconnect_by_data (priv->status_icon, self);
    priv->status_icon = NULL;
    g_object_notify_by_pspec (G_OBJECT (self), props[PHOSH_QUICK_SETTING_PROP_STATUS_ICON]);

    if (priv->box)
      gtk_container_remove (GTK_CONTAINER (priv->box), child);
  } else {
    GTK_CONTAINER_CLASS (phosh_quick_setting_parent_class)->remove (GTK_CONTAINER (self), child);
  }
}


static void
long_pressed_cb (PhoshQuickSetting *self, GtkGesture *gesture)
{
  g_signal_emit (self, signals[LONG_PRESSED], 0);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}


static gboolean
button_pressed_cb (PhoshQuickSetting *self, GdkEventButton *event, GtkButton *button)
{
  if (event->button == 3)
    g_signal_emit (self, signals[LONG_PRESSED], 0);
  return FALSE;
}



static void
phosh_quick_setting_class_init (PhoshQuickSettingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = phosh_quick_setting_get_property;

  /* Override the `add` method, this way we can add the StatusIcon as a child
     in the ui file */
  container_class->add = phosh_quick_setting_add;
  container_class->remove = phosh_quick_setting_remove;

  props[PHOSH_QUICK_SETTING_PROP_STATUS_ICON] =
   g_param_spec_object ("status_icon",
                        "status icon",
                        "The status icon representing the quick setting",
                        PHOSH_TYPE_STATUS_ICON,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PHOSH_QUICK_SETTING_PROP_LAST_PROP, props);

  signals[LONG_PRESSED] = g_signal_new ("long-pressed",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE,
                                        0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/phosh/ui/quick-setting.ui");

  gtk_widget_class_bind_template_child_private (widget_class, PhoshQuickSetting, box);
  gtk_widget_class_bind_template_child_private (widget_class, PhoshQuickSetting, label);
  gtk_widget_class_bind_template_child_private (widget_class, PhoshQuickSetting, long_press);
  gtk_widget_class_bind_template_callback (widget_class, long_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_pressed_cb);

}


static void
phosh_quick_setting_init (PhoshQuickSetting *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
phosh_quick_setting_new (void)
{
  return g_object_new (PHOSH_TYPE_QUICK_SETTING, NULL);
}


PhoshStatusIcon *
phosh_quick_setting_get_status_icon (PhoshQuickSetting *self)
{
  PhoshQuickSettingPrivate *priv;

  g_return_val_if_fail (PHOSH_IS_QUICK_SETTING (self), NULL);

  priv = phosh_quick_setting_get_instance_private (self);

  return priv->status_icon;
}

static void
call_dbus_cb (GDBusProxy *proxy,
              GAsyncResult *res,
              gpointer user_data)
{
  g_autoptr (GError) err = NULL;
  g_autoptr (GVariant) output = NULL;

  output = g_dbus_proxy_call_finish (proxy, res, &err);
  if (err) {
    g_warning ("Can't open panel %s", err->message);
  }
  g_object_unref (proxy);
}

static void
create_dbus_proxy_cb (GObject *source_object, GAsyncResult *res, char *panel)
{
  GDBusProxy *proxy;
  g_autoptr (GError) err = NULL;
  GVariantBuilder builder;
  GVariant *params[3];
  GVariant *array[1];

  proxy = g_dbus_proxy_new_for_bus_finish (res, &err);

  if (err != NULL) {
    g_warning ("Can't open panel %s: %s", panel, err->message);
    g_free (panel);
    return;
  }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));
  g_variant_builder_add (&builder, "v", g_variant_new_string (""));

  array[0] = g_variant_new ("v", g_variant_new ("(sav)", panel, &builder));

  params[0] = g_variant_new_string ("launch-panel");
  params[1] = g_variant_new_array (G_VARIANT_TYPE ("v"), array, 1);
  params[2] = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

  g_dbus_proxy_call (proxy,
		     "Activate",
		     g_variant_new_tuple (params, 3),
		     G_DBUS_CALL_FLAGS_NONE,
		     -1,
		     NULL,
		     (GAsyncReadyCallback) call_dbus_cb,
		     NULL);

  g_free (panel);
}


void
phosh_quick_setting_open_settings_panel (char *panel)
{
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
			    G_DBUS_PROXY_FLAGS_NONE,
			    NULL,
			    "org.gnome.ControlCenter",
			    "/org/gnome/ControlCenter",
			    "org.gtk.Actions",
			    NULL,
			    (GAsyncReadyCallback) create_dbus_proxy_cb,
			    g_strdup (panel));

}
