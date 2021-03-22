/*
 * Copyright © 2020 Zander Brown <zbrown@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Zander Brown <zbrown@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>

#include "notification.h"

G_BEGIN_DECLS


#define PHOSH_TYPE_NOTIFICATION_FRAME (phosh_notification_frame_get_type ())


G_DECLARE_FINAL_TYPE (PhoshNotificationFrame, phosh_notification_frame, PHOSH, NOTIFICATION_FRAME, GtkBox)


GtkWidget         *phosh_notification_frame_new               (void);
void               phosh_notification_frame_bind_notification (PhoshNotificationFrame *self,
                                                               PhoshNotification      *notification);
void               phosh_notification_frame_bind_model        (PhoshNotificationFrame *self,
                                                               GListModel             *model);

G_END_DECLS
