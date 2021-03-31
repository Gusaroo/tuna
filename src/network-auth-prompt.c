/*
 * Copyright (C) 2019 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Mohammed Sadiq <sadiq@sadiqpk.org>
 */

#define G_LOG_DOMAIN "phosh-network-auth-prompt"

#include "config.h"

#include "contrib/shell-network-agent.h"
#include "network-auth-prompt.h"

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>

#include <glib/gi18n.h>

/**
 * SECTION:network-auth-prompt
 * @short_description: A modal prompt for asking Network credentials
 * @Title: PhoshNetworkAuthPrompt
 *
 * The #PhoshNetworkAuthPrompt is used to request network credentials
 * The responses are then passed to NetworkManager
 */

/* TODO: Handle more security methods.  Currently, WEP, WPA/WPA2 personal are supported */

enum {
  DONE,
  N_SIGNALS
};
static guint signals[N_SIGNALS] = { 0 };

struct _PhoshNetworkAuthPrompt
{
  PhoshSystemModalDialog parent;

  GtkWidget      *cancel_button;
  GtkWidget      *connect_button;
  GtkWidget      *message_label;
  GtkWidget      *main_box;

  GtkWidget      *wpa_grid;
  GtkWidget      *wpa_password_entry;
  GcrSecureEntryBuffer *password_buffer;

  NMClient       *nm_client;
  NMConnection   *connection;
  const char     *key_type;
  char           *request_id;
  char           *setting_name;
  NMUtilsSecurityType security_type;
  NMSecretAgentGetSecretsFlags flags;

  ShellNetworkAgent *agent;

  gboolean done_emitted;
  gboolean visible; /* is input visible */
};

G_DEFINE_TYPE(PhoshNetworkAuthPrompt, phosh_network_auth_prompt, PHOSH_TYPE_SYSTEM_MODAL_DIALOG);


static void
emit_done (PhoshNetworkAuthPrompt *self, gboolean cancelled)
{
  g_debug ("Emitting done. Cancelled: %d", cancelled);

  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));

  if (self->done_emitted)
    return;

  self->done_emitted = TRUE;
  g_signal_emit (self, signals[DONE], 0 /* detail */, cancelled);
}


static gboolean
security_has_proto (NMSettingWirelessSecurity *sec, const char *item)
{
  g_return_val_if_fail (sec, FALSE);
  g_return_val_if_fail (item, FALSE);

  for (guint32 i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++) {
    if (strcmp (item, nm_setting_wireless_security_get_proto (sec, i)) == 0)
      return TRUE;
  }

  return FALSE;
}


static NMUtilsSecurityType
network_prompt_get_type (PhoshNetworkAuthPrompt *self)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt, *auth_alg;

  g_return_val_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self), NMU_SEC_NONE);
  g_return_val_if_fail (self->connection, NMU_SEC_NONE);

  setting = nm_connection_get_setting_wireless_security (self->connection);

  if (!setting)
    return NMU_SEC_NONE;

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);
  auth_alg = nm_setting_wireless_security_get_auth_alg (setting);

  if (strcmp (key_mgmt, "none") == 0)
    return NMU_SEC_STATIC_WEP;

  if (strcmp (key_mgmt, "ieee8021x") == 0) {
    if (auth_alg && strcmp (auth_alg, "leap") == 0)
      return NMU_SEC_LEAP;
    return NMU_SEC_DYNAMIC_WEP;
  }

  if (strcmp (key_mgmt, "sae") == 0 ||
      strcmp (key_mgmt, "wpa-none") == 0 ||
      strcmp (key_mgmt, "wpa-psk") == 0) {
    if (security_has_proto (setting, "rsn"))
      return NMU_SEC_WPA2_PSK;
    else
      return NMU_SEC_WPA_PSK;
  }

  if (strcmp (key_mgmt, "wpa-eap") == 0) {
    if (security_has_proto (setting, "rsn"))
      return NMU_SEC_WPA2_ENTERPRISE;
    else
      return NMU_SEC_WPA_ENTERPRISE;
  }

  return NMU_SEC_INVALID;
}


static const char *
network_connection_get_key_type (NMConnection *connection)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt;

  g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

  setting = nm_connection_get_setting_wireless_security (connection);
  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  if (g_str_equal (key_mgmt, "none"))
    return "wep-key0";

  /* Assume WPA/WPA2 Personal */
  return "psk";
}


static void
network_prompt_setup_dialog (PhoshNetworkAuthPrompt *self)
{
  NMSettingWireless *setting;
  g_autofree char *str = NULL;
  g_autofree char *ssid = NULL;
  GBytes *bytes;

  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));

  setting = nm_connection_get_setting_wireless (self->connection);
  self->key_type = network_connection_get_key_type (self->connection);
  self->security_type = network_prompt_get_type (self);

  bytes = nm_setting_wireless_get_ssid (setting);
  ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes));

  if (self->security_type != NMU_SEC_WPA_PSK &&
      self->security_type != NMU_SEC_WPA2_PSK &&
      self->security_type != NMU_SEC_STATIC_WEP) {
    g_debug ("Network security method %d of %s not supported",
             self->security_type, ssid);
    str =  g_strdup_printf(_(
      "Authentication type of wifi network “%s” not supported"), ssid);
    gtk_label_set_label (GTK_LABEL (self->message_label), str);
    return;
  }

  str = g_strdup_printf (_("Enter password for the wifi network “%s”"), ssid);
  gtk_label_set_label (GTK_LABEL (self->message_label), str);

  gtk_container_add (GTK_CONTAINER (self->main_box), self->wpa_grid);

  /* Load password */
  if (self->security_type != NMU_SEC_NONE) {
    NMSettingWirelessSecurity *wireless_setting;
    const char *password = "";

    wireless_setting = nm_connection_get_setting_wireless_security (self->connection);

    if (self->security_type == NMU_SEC_WPA_PSK ||
        self->security_type == NMU_SEC_WPA2_PSK)
      password = nm_setting_wireless_security_get_psk (wireless_setting);
    else if (self->security_type == NMU_SEC_STATIC_WEP) {
      int index;

      index = nm_setting_wireless_security_get_wep_tx_keyidx (wireless_setting);
      password = nm_setting_wireless_security_get_wep_key (wireless_setting, index);
    }

    if (!password)
      password = "";

    gtk_entry_buffer_set_text (GTK_ENTRY_BUFFER (self->password_buffer), password, -1);
  }
  gtk_widget_grab_focus (self->wpa_password_entry);
}


static void
on_dialog_canceled (PhoshNetworkAuthPrompt *self)
{
  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));

  shell_network_agent_respond (self->agent, self->request_id, SHELL_NETWORK_AGENT_USER_CANCELED);
  emit_done (self, TRUE);
}


static void
network_prompt_connect_clicked_cb (PhoshNetworkAuthPrompt *self)
{
  const char *password;

  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));

  password = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (self->password_buffer));
  shell_network_agent_set_password (self->agent, self->request_id,
                                    (char *) self->key_type, (char *) password);
  shell_network_agent_respond (self->agent, self->request_id, SHELL_NETWORK_AGENT_CONFIRMED);

  emit_done (self, FALSE);
}


static void
phosh_network_auth_prompt_finalize (GObject *object)
{
  PhoshNetworkAuthPrompt *self = PHOSH_NETWORK_AUTH_PROMPT (object);

  g_free (self->request_id);
  g_free (self->setting_name);

  g_clear_object (&self->agent);
  g_clear_object (&self->connection);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (phosh_network_auth_prompt_parent_class)->finalize (object);
}


static gboolean
network_prompt_key_press_event_cb (PhoshNetworkAuthPrompt *self,
                                   GdkEventKey            *event)
{
  g_return_val_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self), FALSE);

  if (event->keyval != GDK_KEY_Escape)
    return FALSE;

  emit_done (self, TRUE);
  return TRUE;
}


static void
network_prompt_wpa_password_changed_cb (PhoshNetworkAuthPrompt *self)
{
  const char *password;
  gboolean valid = FALSE;

  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));

  password = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (self->password_buffer));

  if (!password || !*password) {
    /* do nothing */
  } else if (self->security_type == NMU_SEC_WPA_PSK ||
             self->security_type == NMU_SEC_WPA2_PSK) {
    valid = nm_utils_wpa_psk_valid (password);
  } else if (self->security_type == NMU_SEC_STATIC_WEP) {
    valid = nm_utils_wep_key_valid (password, NM_WEP_KEY_TYPE_PASSPHRASE);
    valid |= nm_utils_wep_key_valid (password, NM_WEP_KEY_TYPE_KEY);
  }

  gtk_widget_set_sensitive (self->connect_button, valid);
}


static void
network_prompt_icon_press_cb (PhoshNetworkAuthPrompt *self,
                              GtkEntryIconPosition    icon_pos,
                              GdkEvent               *event,
                              GtkEntry               *entry)
{
  const char *icon_name = "eye-not-looking-symbolic";

  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

  self->visible = !self->visible;
  gtk_entry_set_visibility (entry, self->visible);
  if (self->visible)
    icon_name = "eye-open-negative-filled-symbolic";

  gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY,
                                     icon_name);
}


static void
phosh_network_auth_prompt_class_init (PhoshNetworkAuthPromptClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = phosh_network_auth_prompt_finalize;

 /**
   * PhoshNetworkAuthPrompt::done:
   *
   * This signal is emitted when the prompt can be closed. The cancelled
   * argument indicates whether the prompt was cancelled.
   */
  signals[DONE] = g_signal_new ("done",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/phosh/ui/network-auth-prompt.ui");

  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, connect_button);
  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, message_label);
  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, main_box);

  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, wpa_grid);
  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, wpa_password_entry);
  gtk_widget_class_bind_template_child (widget_class, PhoshNetworkAuthPrompt, password_buffer);

  gtk_widget_class_bind_template_callback (widget_class, on_dialog_canceled);
  gtk_widget_class_bind_template_callback (widget_class, network_prompt_connect_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, network_prompt_key_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, network_prompt_wpa_password_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, network_prompt_icon_press_cb);
}


static void
phosh_network_auth_prompt_init (PhoshNetworkAuthPrompt *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
phosh_network_auth_prompt_new (ShellNetworkAgent *agent,
                               NMClient          *nm_client)
{
  PhoshNetworkAuthPrompt *self;

  g_return_val_if_fail (SHELL_IS_NETWORK_AGENT (agent), NULL);
  g_return_val_if_fail (NM_CLIENT (nm_client), NULL);

  self = g_object_new (PHOSH_TYPE_NETWORK_AUTH_PROMPT, NULL);
  self->nm_client = g_object_ref (nm_client);
  self->agent = g_object_ref (agent);

  return GTK_WIDGET (self);
}


void
phosh_network_auth_prompt_set_request (PhoshNetworkAuthPrompt        *self,
                                       char                          *request_id,
                                       NMConnection                  *connection,
                                       char                          *setting_name,
                                       char                         **hints,
                                       NMSecretAgentGetSecretsFlags   flags)
{
  g_return_if_fail (PHOSH_IS_NETWORK_AUTH_PROMPT (self));
  g_return_if_fail (NM_IS_CONNECTION (connection));

  g_free (self->request_id);
  g_free (self->setting_name);
  self->request_id = g_strdup (request_id);
  self->setting_name = g_strdup (setting_name);
  g_set_object (&self->connection, connection);

  network_prompt_setup_dialog (self);
}
