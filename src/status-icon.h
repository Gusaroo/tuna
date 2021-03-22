/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PHOSH_TYPE_STATUS_ICON (phosh_status_icon_get_type())

G_DECLARE_DERIVABLE_TYPE (PhoshStatusIcon, phosh_status_icon, PHOSH, STATUS_ICON, GtkBin)

struct _PhoshStatusIconClass
{
  GtkBinClass parent_class;
};

GtkWidget * phosh_status_icon_new (void);
void phosh_status_icon_set_icon_size (PhoshStatusIcon *self, GtkIconSize size);
GtkIconSize phosh_status_icon_get_icon_size (PhoshStatusIcon *self);
void phosh_status_icon_set_icon_name (PhoshStatusIcon *self, const char *icon_name);
char *phosh_status_icon_get_icon_name (PhoshStatusIcon *self);
void phosh_status_icon_set_extra_widget (PhoshStatusIcon *self, GtkWidget *widget);
GtkWidget * phosh_status_icon_get_extra_widget (PhoshStatusIcon *self);
void phosh_status_icon_set_info (PhoshStatusIcon *self, const char *info);
char *phosh_status_icon_get_info (PhoshStatusIcon *self);
void phosh_status_icon_set_show_always (PhoshStatusIcon *self, gboolean show_always);
gboolean phosh_status_icon_get_show_always (PhoshStatusIcon *self);

G_END_DECLS
