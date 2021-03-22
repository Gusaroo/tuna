/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define PHOSH_TYPE_WIFI_MANAGER (phosh_wifi_manager_get_type())

G_DECLARE_FINAL_TYPE (PhoshWifiManager, phosh_wifi_manager, PHOSH, WIFI_MANAGER, GObject)

PhoshWifiManager  *phosh_wifi_manager_new (void);
guint8             phosh_wifi_manager_get_strength (PhoshWifiManager *self);
const char        *phosh_wifi_manager_get_icon_name (PhoshWifiManager *self);
const char        *phosh_wifi_manager_get_ssid (PhoshWifiManager *self);
gboolean           phosh_wifi_manager_get_enabled (PhoshWifiManager *self);
gboolean           phosh_wifi_manager_get_present (PhoshWifiManager *self);

G_END_DECLS
