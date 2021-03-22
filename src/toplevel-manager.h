/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "toplevel.h"

#define PHOSH_TYPE_TOPLEVEL_MANAGER (phosh_toplevel_manager_get_type())

G_DECLARE_FINAL_TYPE (PhoshToplevelManager,
                      phosh_toplevel_manager,
                      PHOSH,
                      TOPLEVEL_MANAGER,
                      GObject)

PhoshToplevel        *phosh_toplevel_manager_get_toplevel (PhoshToplevelManager *self, guint num);
guint                 phosh_toplevel_manager_get_num_toplevels (PhoshToplevelManager *self);
PhoshToplevelManager *phosh_toplevel_manager_new (void);
