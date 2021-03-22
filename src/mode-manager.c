/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "phosh-mode-manager"

#include "config.h"

#include "mode-manager.h"
#include "shell.h"
#include "dbus/hostname1-dbus.h"

#define BUS_NAME "org.freedesktop.hostname1"
#define OBJECT_PATH "/org/freedesktop/hostname1"

#define PHOC_KEY_MAXIMIZE "auto-maximize"
#define A11Y_KEY_OSK "screen-keyboard-enabled"
#define WM_KEY_LAYOUT "button-layout"

/**
 * SECTION:mode-manager
 * @short_description: Determines the device mode
 * @Title: PhoshModeManager
 *
 * #PhoshModeManager tracks the device mode and attached hardware.
 */

enum {
  PROP_0,
  PROP_DEVICE_TYPE,
  PROP_HW_FLAGS,
  PROP_MIMICRY,

  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _PhoshModeManager {
  GObject                      parent;

  PhoshModeDeviceType          device_type;
  PhoshModeDeviceType          mimicry;
  PhoshModeHwFlags             hw_flags;

  PhoshMonitorManager         *monitor_manager;

  PhoshHostname1DBusHostname1 *proxy;
  gchar                       *chassis;
  PhoshWaylandSeatCapabilities wl_caps;
};
G_DEFINE_TYPE (PhoshModeManager, phosh_mode_manager, G_TYPE_OBJECT);


static void
phosh_mode_manager_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PhoshModeManager *self = PHOSH_MODE_MANAGER (object);

  switch (property_id) {
  case PROP_HW_FLAGS:
    g_value_set_flags (value, self->hw_flags);
    break;
  case PROP_DEVICE_TYPE:
    g_value_set_enum (value, self->device_type);
    break;
  case PROP_MIMICRY:
    g_value_set_enum (value, self->mimicry);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
update_props (PhoshModeManager *self)
{
  int n_monitors;
  PhoshModeDeviceType device_type, mimicry;
  PhoshModeHwFlags hw;
  PhoshShell *shell = phosh_shell_get_default ();

  /* Self->Chassis type */
  hw = PHOSH_MODE_HW_NONE;
  if (g_strcmp0 (self->chassis, "handset") == 0) {
    device_type = PHOSH_MODE_DEVICE_TYPE_PHONE;
  } else if (g_strcmp0 (self->chassis, "laptop") == 0) {
    device_type = PHOSH_MODE_DEVICE_TYPE_LAPTOP;
    hw |= PHOSH_MODE_HW_KEYBOARD;
  } else if (g_strcmp0 (self->chassis, "desktop") == 0) {
    device_type = PHOSH_MODE_DEVICE_TYPE_DESKTOP;
    hw |= PHOSH_MODE_HW_KEYBOARD;
  } else if (g_strcmp0 (self->chassis, "convertible") == 0) {
    device_type = PHOSH_MODE_DEVICE_TYPE_CONVERTIBLE;
  } else if (g_strcmp0 (self->chassis, "tablet") == 0) {
    device_type = PHOSH_MODE_DEVICE_TYPE_TABLET;
  } else {
    device_type = PHOSH_MODE_DEVICE_TYPE_UNKNOWN;
  }
  mimicry = device_type;

  /* Additional hardware */
  n_monitors = phosh_monitor_manager_get_num_monitors (self->monitor_manager);
  if (n_monitors > 1 || (n_monitors == 1 && phosh_shell_get_builtin_monitor (shell) !=
                         phosh_shell_get_primary_monitor (shell))) {
    hw |= PHOSH_MODE_HW_EXT_DISPLAY;
  }

  if (self->wl_caps & PHOSH_WAYLAND_SEAT_CAPABILITY_POINTER)
    hw |= PHOSH_MODE_HW_POINTER;

  /* Mimicries */
  if (device_type == PHOSH_MODE_DEVICE_TYPE_PHONE &&
      (hw & PHOSH_MODE_DOCKED_PHONE_MASK) == PHOSH_MODE_DOCKED_PHONE_MASK) {
    mimicry = PHOSH_MODE_DEVICE_TYPE_DESKTOP;
  } else if (device_type == PHOSH_MODE_DEVICE_TYPE_TABLET &&
      (hw & PHOSH_MODE_DOCKED_TABLET_MASK) == PHOSH_MODE_DOCKED_TABLET_MASK) {
    mimicry = PHOSH_MODE_DEVICE_TYPE_DESKTOP;
  }

  g_object_freeze_notify (G_OBJECT (self));

  if (device_type != self->device_type) {
    g_autofree char *name = g_enum_to_string (PHOSH_TYPE_MODE_DEVICE_TYPE, device_type);

    self->device_type = device_type;
    g_debug ("Device type is %s", name);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEVICE_TYPE]);
  }

  if (mimicry != self->mimicry) {
    g_autofree char *name = g_enum_to_string (PHOSH_TYPE_MODE_DEVICE_TYPE, mimicry);

    self->mimicry = mimicry;
    g_debug ("Mimicry is %s", name);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MIMICRY]);
  }

  if (hw != self->hw_flags) {
    g_autofree char *names = g_flags_to_string (PHOSH_TYPE_MODE_HW_FLAGS, hw);
    self->hw_flags = hw;
    g_debug ("HW flags %s", names);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HW_FLAGS]);
  }

  g_object_thaw_notify (G_OBJECT (self));
}


static void
on_n_monitors_changed (PhoshModeManager *self, GParamSpec *pspec, PhoshMonitorManager *manager)
{
  g_return_if_fail (PHOSH_IS_MODE_MANAGER (self));
  g_return_if_fail (PHOSH_IS_MONITOR_MANAGER (manager));

  update_props (self);
}


static void
on_chassis_changed (PhoshModeManager            *self,
                    GParamSpec                  *pspec,
                    PhoshHostname1DBusHostname1 *proxy)
{
  const gchar *chassis;

  g_return_if_fail (PHOSH_IS_MODE_MANAGER (self));
  g_return_if_fail (PHOSH_HOSTNAME1_DBUS_IS_HOSTNAME1 (proxy));

  chassis = phosh_hostname1_dbus_hostname1_get_chassis (self->proxy);

  if (!chassis)
    return;

  g_debug ("Chassis: %s", chassis);
  g_free (self->chassis);
  self->chassis = g_strdup (chassis);
  update_props (self);
}


static void
on_seat_capabilities_changed (PhoshModeManager *self,
                              GParamSpec       *pspec,
                              PhoshWayland     *wl)
{
  g_return_if_fail (PHOSH_IS_MODE_MANAGER (self));
  g_return_if_fail (PHOSH_IS_WAYLAND (wl));

  self->wl_caps = phosh_wayland_get_seat_capabilities (wl);
  update_props (self);
}


static void
on_proxy_new_for_bus_finish (GObject          *source_object,
                             GAsyncResult     *res,
                             PhoshModeManager *self)
{
  g_autoptr (GError) err = NULL;
  PhoshWayland *wl = phosh_wayland_get_default ();

  g_return_if_fail (PHOSH_IS_MODE_MANAGER (self));

  self->proxy = phosh_hostname1_dbus_hostname1_proxy_new_for_bus_finish (res, &err);
  if (!self->proxy) {
    g_warning ("Failed to get hostname1 proxy: %s", err->message);
    goto out;
  }

  g_debug ("Hostname1 interface initialized");
  g_signal_connect_object (self->proxy,
                           "notify::chassis",
                           G_CALLBACK (on_chassis_changed),
                           self,
                           G_CONNECT_SWAPPED);
  on_chassis_changed (self, NULL, self->proxy);

  g_signal_connect_object (wl,
                           "notify::seat-capabilities",
                           G_CALLBACK (on_seat_capabilities_changed),
                           self,
                           G_CONNECT_SWAPPED);
  on_seat_capabilities_changed (self, NULL, wl);

  g_signal_connect_object (self->monitor_manager,
                           "notify::n-monitors",
                           G_CALLBACK (on_n_monitors_changed),
                           self,
                           G_CONNECT_SWAPPED);
  /* n_monitors is always updated in update_props () */

out:
  g_object_unref (self);
}


static gboolean
on_idle (PhoshTorchManager *self)
{
  phosh_hostname1_dbus_hostname1_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    BUS_NAME,
                                                    OBJECT_PATH,
                                                    NULL,
                                                    (GAsyncReadyCallback) on_proxy_new_for_bus_finish,
                                                    g_object_ref (self));

  return G_SOURCE_REMOVE;
}

static void
phosh_mode_manager_constructed (GObject *object)
{
  PhoshModeManager *self = PHOSH_MODE_MANAGER (object);

  G_OBJECT_CLASS (phosh_mode_manager_parent_class)->constructed (object);

  self->monitor_manager = phosh_shell_get_monitor_manager (phosh_shell_get_default ());

  /* Perform DBus setup when idle */
  g_idle_add ((GSourceFunc)on_idle, self);
}


static void
phosh_mode_manager_dispose (GObject *object)
{
  PhoshModeManager *self = PHOSH_MODE_MANAGER (object);

  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (phosh_mode_manager_parent_class)->dispose (object);
}


static void
phosh_mode_manager_finalize (GObject *object)
{
  PhoshModeManager *self = PHOSH_MODE_MANAGER (object);

  g_clear_pointer (&self->chassis, g_free);

  G_OBJECT_CLASS (phosh_mode_manager_parent_class)->finalize (object);
}


static void
phosh_mode_manager_class_init (PhoshModeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = phosh_mode_manager_constructed;
  object_class->dispose = phosh_mode_manager_dispose;
  object_class->finalize = phosh_mode_manager_finalize;
  object_class->get_property = phosh_mode_manager_get_property;

  props[PROP_DEVICE_TYPE] =
    g_param_spec_enum ("device-type",
                       "Device Type",
                       "The device type",
                       PHOSH_TYPE_MODE_DEVICE_TYPE,
                       PHOSH_MODE_DEVICE_TYPE_PHONE,
                       G_PARAM_READABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  props[PROP_HW_FLAGS] =
    g_param_spec_flags ("hw-flags",
                        "Hardware flags",
                        "Flags for available hardware",
                        PHOSH_TYPE_MODE_HW_FLAGS,
                        PHOSH_MODE_HW_NONE,
                        G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * PhoshMode:device-mimicry:
   *
   * What this device plus external hardware should be handled
   * like. E.g. a phone with keyboard and mouse and 2nd screen looks
   * much like a desktop. A touch laptop with removable keyboard can
   * look like a tablet.
   */
  props[PROP_MIMICRY] =
    g_param_spec_enum ("mimicry",
                       "Device Mimicry",
                       "The device mimicry",
                       PHOSH_TYPE_MODE_DEVICE_TYPE,
                       PHOSH_MODE_DEVICE_TYPE_PHONE,
                       G_PARAM_READABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
phosh_mode_manager_init (PhoshModeManager *self)
{
  self->hw_flags = PHOSH_MODE_HW_NONE;
  self->device_type = PHOSH_MODE_DEVICE_TYPE_UNKNOWN;
  self->mimicry = PHOSH_MODE_DEVICE_TYPE_UNKNOWN;
}


PhoshModeManager *
phosh_mode_manager_new (void)
{
  return PHOSH_MODE_MANAGER (g_object_new (PHOSH_TYPE_MODE_MANAGER, NULL));
}


PhoshModeDeviceType
phosh_mode_manager_get_device_type (PhoshModeManager *self)
{
  g_return_val_if_fail (PHOSH_IS_MODE_MANAGER (self), PHOSH_MODE_DEVICE_TYPE_UNKNOWN);

  return self->device_type;
}


PhoshModeDeviceType
phosh_mode_manager_get_mimicry (PhoshModeManager *self)
{
  g_return_val_if_fail (PHOSH_IS_MODE_MANAGER (self), PHOSH_MODE_DEVICE_TYPE_UNKNOWN);

  return self->mimicry;
}
