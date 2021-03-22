/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#define PHOSH_TYPE_OVERVIEW (phosh_overview_get_type())

G_DECLARE_FINAL_TYPE (PhoshOverview, phosh_overview, PHOSH, OVERVIEW, GtkBox)


GtkWidget *phosh_overview_new (void);
void       phosh_overview_reset (PhoshOverview *self);
void       phosh_overview_focus_app_search (PhoshOverview *self);
gboolean   phosh_overview_has_running_activities (PhoshOverview *self);
