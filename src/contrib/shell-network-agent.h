/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_NETWORK_AGENT_H__
#define __SHELL_NETWORK_AGENT_H__

#include <glib-object.h>
#include <glib.h>
#include <NetworkManager.h>
#include <nm-secret-agent-old.h>

G_BEGIN_DECLS

/**
 * ShellNetworkAgentResponse:
 */
typedef enum {
  /*<private >*/
  SHELL_NETWORK_AGENT_CONFIRMED,
  SHELL_NETWORK_AGENT_USER_CANCELED,
  SHELL_NETWORK_AGENT_INTERNAL_ERROR
} ShellNetworkAgentResponse;

typedef struct _ShellNetworkAgent         ShellNetworkAgent;
typedef struct _ShellNetworkAgentClass    ShellNetworkAgentClass;
typedef struct _ShellNetworkAgentPrivate  ShellNetworkAgentPrivate;

#define SHELL_TYPE_NETWORK_AGENT                  (shell_network_agent_get_type ())
#define SHELL_NETWORK_AGENT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_NETWORK_AGENT, ShellNetworkAgent))
#define SHELL_IS_NETWORK_AGENT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_NETWORK_AGENT))
#define SHELL_NETWORK_AGENT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_NETWORK_AGENT, ShellNetworkAgentClass))
#define SHELL_IS_NETWORK_AGENT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_NETWORK_AGENT))
#define SHELL_NETWORK_AGENT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_NETWORK_AGENT, ShellNetworkAgentClass))

struct _ShellNetworkAgent
{
  /*< private >*/
  NMSecretAgentOld parent_instance;

  ShellNetworkAgentPrivate *priv;
};

struct _ShellNetworkAgentClass
{
  /*< private >*/
  NMSecretAgentOldClass parent_class;
};

/* used by SHELL_TYPE_NETWORK_AGENT */
GType shell_network_agent_get_type (void);

void               shell_network_agent_set_password (ShellNetworkAgent *self,
                                                     char              *request_id,
                                                     char              *setting_key,
                                                     char              *setting_value);
void               shell_network_agent_respond      (ShellNetworkAgent *self,
                                                     char              *request_id,
                                                     ShellNetworkAgentResponse response);

/* If these are kept in sync with nm-applet, secrets will be shared */
#define SHELL_KEYRING_UUID_TAG "connection-uuid"
#define SHELL_KEYRING_SN_TAG "setting-name"
#define SHELL_KEYRING_SK_TAG "setting-key"

G_END_DECLS

#endif /* __SHELL_NETWORK_AGENT_H__ */
