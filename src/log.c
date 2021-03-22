/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "phosh-log"

#include "config.h"
#include "log.h"

#include <stdio.h>

/* these are emitted by the default log handler */
#define DEFAULT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE)
/* these are filtered by G_MESSAGES_DEBUG by the default log handler */
#define INFO_LEVELS (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)

static gboolean
log_is_old_api (const GLogField *fields,
                gsize            n_fields)
{
  return (n_fields >= 1 &&
          g_strcmp0 (fields[0].key, "GLIB_OLD_LOG_API") == 0 &&
          g_strcmp0 (fields[0].value, "1") == 0);
}


static void
_phosh_log_abort (gboolean breakpoint)
{
  if (breakpoint)
    G_BREAKPOINT ();
  else
    g_abort ();
}


static GLogWriterOutput
phosh_log_writer_default (GLogLevelFlags   log_level,
                          const GLogField *fields,
                          gsize            n_fields,
                          gchar           *log_domains)
{
  static gsize initialized = 0;
  static gboolean stderr_is_journal = FALSE;
  GLogLevelFlags always_fatal;

  g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
  g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

  /* Disable debug message output unless specified in log_domains. */
  if (!(log_level & DEFAULT_LEVELS) && !(log_level >> G_LOG_LEVEL_USER_SHIFT)) {
    const gchar *log_domain = NULL;
    gsize i;

    if ((log_level & INFO_LEVELS) == 0 || log_domains == NULL) {
      return G_LOG_WRITER_HANDLED;
    }

    for (i = 0; i < n_fields; i++) {
      if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0) {
        log_domain = fields[i].value;
        break;
      }
    }

    if (strcmp (log_domains, "all") != 0 &&
        (log_domain == NULL || !strstr (log_domains, log_domain))) {
      return G_LOG_WRITER_HANDLED;
    }
  }

  /* Need to retrieve this from glib via getting and resetting:
   * https://gitlab.gnome.org/GNOME/glib/-/issues/2217 */
  always_fatal = g_log_set_always_fatal (0);
  g_log_set_always_fatal (always_fatal);

  if ((log_level & always_fatal) && !log_is_old_api (fields, n_fields))
    log_level |= G_LOG_FLAG_FATAL;

  /* Try logging to the systemd journal as first choice. */
  if (g_once_init_enter (&initialized)) {
    stderr_is_journal = g_log_writer_is_journald (fileno (stderr));
    g_once_init_leave (&initialized, TRUE);
  }

  if (stderr_is_journal &&
      g_log_writer_journald (log_level, fields, n_fields, log_domains) ==
      G_LOG_WRITER_HANDLED)
    goto handled;

  if (g_log_writer_standard_streams (log_level, fields, n_fields, log_domains) ==
      G_LOG_WRITER_HANDLED)
    goto handled;

  return G_LOG_WRITER_UNHANDLED;

handled:
  /* Abort if the message was fatal. */
  if (log_level & G_LOG_FLAG_FATAL) {
    _phosh_log_abort (!(log_level & G_LOG_FLAG_RECURSION));
  }

  return G_LOG_WRITER_HANDLED;
}

/**
 * phosh_log_set_log_domains:
 * @domains: comma separated list of log domains. This string must
 *  remain valid during the programs life time.
 *
 * Set the current logging domains.
 */
void
phosh_log_set_log_domains (const char *domains)
{
  /* We replace the whole writer instead of just updating domains
     since that is already mutex protected */
  g_log_set_writer_func ((GLogWriterFunc)phosh_log_writer_default,
                         (char*)domains, NULL);
}
