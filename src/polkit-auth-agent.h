/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#pragma once

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>

#include <glib-object.h>

G_BEGIN_DECLS

/* libpolkit lacks these */
#if POLKIT_AGENT_MAJOR_VERSION == 0 && POLKIT_AGENT_MINOR_VERSION < 114
G_DEFINE_AUTOPTR_CLEANUP_FUNC(PolkitSubject, g_object_unref)
#endif
G_DEFINE_AUTOPTR_CLEANUP_FUNC(PolkitAgentListener, g_object_unref)

#define PHOSH_TYPE_POLKIT_AUTH_AGENT (phosh_polkit_auth_agent_get_type())

G_DECLARE_FINAL_TYPE (PhoshPolkitAuthAgent, phosh_polkit_auth_agent, PHOSH, POLKIT_AUTH_AGENT, PolkitAgentListener)

PhoshPolkitAuthAgent * phosh_polkit_auth_agent_new           (void);
void                   phosh_polkit_authentication_agent_register (PhoshPolkitAuthAgent *agent,
                                                                   GError              **error);

G_END_DECLS
