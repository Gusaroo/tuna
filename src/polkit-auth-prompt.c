/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * Based on gnome-shell's shell-keyring-prompt.c
 * Author: Stef Walter <stefw@gnome.org>
 */

#define G_LOG_DOMAIN "phosh-polkit-auth-prompt"

#include "config.h"

#include "polkit-auth-prompt.h"

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>

#include <glib/gi18n.h>

/**
 * SECTION:polkit-auth-prompt
 * @short_description: A modal prompt for policy kit authentication
 * @Title: PhoshPolkitAuthPrompt
 *
 * The #PhoshPolkitAuthPrompt is used to ask policy kit authentication.
 * This handles the interaction with PolkitAgentSession.
 */

enum {
  PROP_0,
  PROP_ACTION_ID,
  PROP_COOKIE,
  PROP_MESSAGE,
  PROP_ICON_NAME,
  PROP_USER_NAMES,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  DONE,
  N_SIGNALS
};
static guint signals[N_SIGNALS] = { 0 };

struct _PhoshPolkitAuthPrompt
{
  PhoshSystemModalDialog parent;

  GtkWidget *lbl_message;
  GtkWidget *lbl_user_name;
  GtkWidget *lbl_password;
  GtkWidget *lbl_info;
  GtkWidget *img_icon;
  GtkWidget *btn_authenticate;
  GtkWidget *spinner_authenticate;
  GtkWidget *btn_cancel;
  GtkWidget *entry_password;
  GtkEntryBuffer *password_buffer;

  char  *action_id;
  char  *message;
  char  *icon_name;
  char  *cookie;
  GStrv  user_names;

  PolkitIdentity *identity;
  PolkitAgentSession *session;

  gboolean done_emitted;
};
G_DEFINE_TYPE(PhoshPolkitAuthPrompt, phosh_polkit_auth_prompt, PHOSH_TYPE_SYSTEM_MODAL_DIALOG);


static void phosh_polkit_auth_prompt_initiate (PhoshPolkitAuthPrompt *self);


static void
set_action_id (PhoshPolkitAuthPrompt *self, const char *action_id)
{
  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  if (!g_strcmp0 (self->action_id, action_id))
    return;

  g_clear_pointer (&self->action_id, g_free);
  self->action_id = g_strdup (action_id);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTION_ID]);
}


static void
set_cookie (PhoshPolkitAuthPrompt *self, const char *cookie)
{
  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  if (!g_strcmp0 (self->cookie, cookie))
    return;

  g_clear_pointer (&self->cookie, g_free);
  self->cookie = g_strdup (cookie);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COOKIE]);
}


static void
set_message (PhoshPolkitAuthPrompt *self, const char *message)
{
  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  if (!g_strcmp0 (self->message, message))
    return;

  g_clear_pointer (&self->message, g_free);
  self->message = g_strdup (message);
  gtk_label_set_label (GTK_LABEL (self->lbl_message), message ?: "");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MESSAGE]);
}


static void
set_icon_name (PhoshPolkitAuthPrompt *self, const char *icon_name)
{
  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  if (!g_strcmp0 (self->icon_name, icon_name))
    return;

  g_clear_pointer (&self->icon_name, g_free);
  self->icon_name = g_strdup (icon_name);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->img_icon),
                                (icon_name && strlen(icon_name)) ? icon_name : "dialog-password-symbolic",
                                GTK_ICON_SIZE_DIALOG);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ICON_NAME]);
}

static void
set_user_names (PhoshPolkitAuthPrompt *self, const GStrv user_names)
{
  const char *user_name = NULL;
  GError *err = NULL;
  int len;

  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  /* if both are NULL */
  if (self->user_names == user_names)
    return;

  g_strfreev (self->user_names);
  self->user_names = g_strdupv (user_names);

  len = g_strv_length (self->user_names);
  if (len == 0) {
    user_name = "unknown";
  } else if (len == 1) {
    user_name = self->user_names[0];
  } else {
    g_debug ("Received %d user names, only using one", len);
    user_name = g_get_user_name();

    if (!g_strv_contains ((const char *const *)user_names, user_name))
      user_name = "root";

    if (!g_strv_contains ((const char *const *)user_names, user_name))
      user_name = user_names[0];
  };

  self->identity = polkit_unix_user_new_for_name (user_name, &err);
  if (!self->identity) {
    g_warning("Failed to create identity: %s", err->message);
    g_clear_error (&err);
    return;
  }

  gtk_label_set_text (GTK_LABEL (self->lbl_user_name), user_name);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USER_NAMES]);
}


static void
phosh_polkit_auth_prompt_set_property (GObject      *obj,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  PhoshPolkitAuthPrompt *self = PHOSH_POLKIT_AUTH_PROMPT (obj);

  switch (prop_id) {
  case PROP_ACTION_ID:
    set_action_id (self, g_value_get_string(value));
    break;
  case PROP_COOKIE:
    set_cookie (self, g_value_get_string(value));
    break;
  case PROP_MESSAGE:
    set_message (self, g_value_get_string(value));
    break;
  case PROP_ICON_NAME:
    set_icon_name (self, g_value_get_string(value));
    break;
  case PROP_USER_NAMES:
    set_user_names (self, g_value_get_boxed(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}


static void
phosh_polkit_auth_prompt_get_property (GObject    *obj,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  PhoshPolkitAuthPrompt *self = PHOSH_POLKIT_AUTH_PROMPT (obj);

  switch (prop_id) {
  case PROP_ACTION_ID:
    g_value_set_string (value, self->action_id?: "");
    break;
  case PROP_COOKIE:
    g_value_set_string (value, self->cookie?: "");
    break;
  case PROP_MESSAGE:
    g_value_set_string (value, self->message?: "");
    break;
  case PROP_ICON_NAME:
    g_value_set_string (value, self->icon_name?: "");
    break;
  case PROP_USER_NAMES:
    g_value_set_boxed (value, self->user_names ? g_strdupv(self->user_names) : NULL);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}


static void
emit_done (PhoshPolkitAuthPrompt *self, gboolean cancelled)
{
  g_debug ("Emitting done. Cancelled: %d", cancelled);

  if (!self->done_emitted) {
    self->done_emitted = TRUE;
    g_signal_emit (self, signals[DONE], 0 /* detail */, cancelled);
  }
}


static void
on_auth_session_request (PhoshPolkitAuthPrompt *self,
                         char                  *request,
                         gboolean               echo_on,
                         gpointer               user_data)
{
  g_debug("Request: %s, echo: %d", request, echo_on);

  if (self->done_emitted)
    return;

  /* Translate the most common request */
  if (!g_strcmp0 (request, "Password: ") || !g_strcmp0 (request, "Password:"))
    request = _("Password:");

  gtk_label_set_text(GTK_LABEL (self->lbl_password), request);
  gtk_entry_set_visibility (GTK_ENTRY (self->entry_password), echo_on);
  gtk_entry_set_text (GTK_ENTRY (self->entry_password), "");
  gtk_widget_grab_focus (self->entry_password);
}

static void
on_auth_session_show_error (PhoshPolkitAuthPrompt *self,
                            char                  *text,
                            PolkitAgentSession    *session)
{
  g_debug ("%s", text);
  gtk_entry_set_text (GTK_ENTRY (self->lbl_info), text);
}


static void
on_auth_session_show_info (PhoshPolkitAuthPrompt *self,
                           char                  *text,
                           PolkitAgentSession    *session)
{
  g_debug ("%s", text);
  gtk_entry_set_text (GTK_ENTRY (self->lbl_info), text);
}


static void
on_auth_session_completed (PhoshPolkitAuthPrompt *self,
                           gboolean               gained_auth,
                           PolkitAgentSession    *session)
{
  g_debug ("Gained auth: %d", gained_auth);
  gtk_spinner_stop (GTK_SPINNER (self->spinner_authenticate));
  gtk_widget_hide (self->spinner_authenticate);

  if (self->done_emitted)
    return;

  if (gained_auth) {
      emit_done (self, FALSE);
  } else {
    const char *info = gtk_label_get_text (GTK_LABEL (self->lbl_info));

    if (info == NULL || !g_strcmp0(info, "")) {
      gtk_label_set_text (GTK_LABEL (self->lbl_info),
                          _("Sorry, that didn’t work. Please try again."));
    }
    /* Start new auth session */
    g_signal_handlers_disconnect_by_data (session, self);
    phosh_polkit_auth_prompt_initiate (self);
  }
}


static void
phosh_polkit_auth_prompt_initiate (PhoshPolkitAuthPrompt *self)
{
  g_return_if_fail (self->identity);
  g_return_if_fail (self->cookie);

  self->session = polkit_agent_session_new (self->identity,
                                            self->cookie);

  g_signal_connect_swapped (self->session,
                            "request",
                            G_CALLBACK (on_auth_session_request),
                            self);

  g_signal_connect_swapped (self->session,
                            "show-error",
                            G_CALLBACK (on_auth_session_show_error),
                            self);

  g_signal_connect_swapped (self->session,
                            "show-info",
                            G_CALLBACK (on_auth_session_show_info),
                            self);

  g_signal_connect_swapped (self->session,
                            "completed",
                            G_CALLBACK (on_auth_session_completed),
                            self);

  polkit_agent_session_initiate (self->session);
}


static void
on_dialog_canceled (PhoshPolkitAuthPrompt *self, gpointer unused)
{
  g_return_if_fail (PHOSH_IS_POLKIT_AUTH_PROMPT (self));

  polkit_agent_session_cancel (self->session);
  emit_done (self, TRUE);
}


static void
on_btn_authenticate_clicked (PhoshPolkitAuthPrompt *self, GtkButton *btn)
{
  const char *password;

  password = gtk_entry_buffer_get_text (self->password_buffer);

  if (!password || strlen (password) == 0)
    return;

  gtk_label_set_text (GTK_LABEL (self->lbl_info), "");
  gtk_widget_show (self->spinner_authenticate);
  gtk_spinner_start (GTK_SPINNER (self->spinner_authenticate));
  polkit_agent_session_response (self->session, password);
}


static void
phosh_polkit_auth_prompt_dispose (GObject *obj)
{
  PhoshPolkitAuthPrompt *self = PHOSH_POLKIT_AUTH_PROMPT (obj);

  g_clear_object (&self->identity);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (phosh_polkit_auth_prompt_parent_class)->dispose (obj);
}


static void
phosh_polkit_auth_prompt_finalize (GObject *obj)
{
  PhoshPolkitAuthPrompt *self = PHOSH_POLKIT_AUTH_PROMPT (obj);

  g_free (self->message);
  g_free (self->icon_name);
  g_free (self->action_id);
  g_free (self->cookie);
  g_strfreev (self->user_names);

  G_OBJECT_CLASS (phosh_polkit_auth_prompt_parent_class)->finalize (obj);
}


static void
phosh_polkit_auth_prompt_constructed (GObject *object)
{
  PhoshPolkitAuthPrompt *self = PHOSH_POLKIT_AUTH_PROMPT (object);

  G_OBJECT_CLASS (phosh_polkit_auth_prompt_parent_class)->constructed (object);

  self->password_buffer = gcr_secure_entry_buffer_new ();
  gtk_entry_set_buffer (GTK_ENTRY (self->entry_password),
                        GTK_ENTRY_BUFFER (self->password_buffer));

  g_signal_connect_object (self->btn_cancel,
                           "clicked",
                           G_CALLBACK (on_dialog_canceled),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->btn_authenticate,
                           "clicked",
                           G_CALLBACK (on_btn_authenticate_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  phosh_polkit_auth_prompt_initiate (self);
}


static void
phosh_polkit_auth_prompt_class_init (PhoshPolkitAuthPromptClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = phosh_polkit_auth_prompt_get_property;
  object_class->set_property = phosh_polkit_auth_prompt_set_property;
  object_class->constructed = phosh_polkit_auth_prompt_constructed;
  object_class->dispose = phosh_polkit_auth_prompt_dispose;
  object_class->finalize = phosh_polkit_auth_prompt_finalize;

  props[PROP_ACTION_ID] =
    g_param_spec_string ("action-id",
                         "Action ID",
                         "The prompt's action id",
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_COOKIE] =
    g_param_spec_string ("cookie",
                         "Cookie",
                         "The prompt's cookie",
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "The prompt's message",
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon name",
                         "The prompt's icon name",
                         "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_USER_NAMES] =
    g_param_spec_boxed ("user-names",
                        "User names",
                        "The user name's to authenticate as",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

 /**
   * PhoshPolkitAuthPrompt::done:
   * @self: The #PhoshPolkitAuthPrompt instance.
   *
   * This signal is emitted when the prompt can be closed. The cancelled
   * argument indicates whether the prompt was cancelled.
   */
  signals[DONE] = g_signal_new (
    "done", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
    NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/phosh/ui/polkit-auth-prompt.ui");
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, lbl_message);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, lbl_user_name);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, lbl_password);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, lbl_info);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, img_icon);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, btn_authenticate);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, btn_cancel);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, entry_password);
  gtk_widget_class_bind_template_child (widget_class, PhoshPolkitAuthPrompt, spinner_authenticate);
  gtk_widget_class_bind_template_callback (widget_class, on_dialog_canceled);
}


static void
phosh_polkit_auth_prompt_init (PhoshPolkitAuthPrompt *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
phosh_polkit_auth_prompt_new (const char *action_id,
                              const char *message,
                              const char *icon_name,
                              const char *cookie,
                              GStrv user_names)
{
  return g_object_new (PHOSH_TYPE_POLKIT_AUTH_PROMPT,
                       /* polkit prompt */
                       "action-id", action_id,
                       "cookie", cookie,
                       "message", message,
                       "icon-name", icon_name,
                       "user-names", user_names,
                       NULL);
}
