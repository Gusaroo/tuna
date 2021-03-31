/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#include "testlib.h"

#include "layersurface.h"
#include <gdk/gdkwayland.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <stdint.h>

typedef struct _PhocOutputWatch {
  char      *socket;
  GMainLoop *loop;
} PhocOutputWatch;

static gboolean
phoc_stdout_watch (GIOChannel      *source,
                   GIOCondition     condition,
                   PhocOutputWatch *watch)
{
  gboolean finished = FALSE;

  if (condition & G_IO_IN) {
    GIOStatus status;
    g_autoptr (GError) err = NULL;
    g_autofree char *line = NULL;

    line = NULL;
    status = g_io_channel_read_line (source, &line, NULL, NULL, &err);

    switch (status) {
    case G_IO_STATUS_NORMAL:
    {
      g_autoptr (GRegex) socket_re = NULL;
      g_autoptr (GMatchInfo) socket_mi = NULL;

      g_debug ("Phoc startup msg: %s", line);

      socket_re = g_regex_new ("on wayland display '(wayland-[0-9]+)'",
                               0, 0, &err);
      g_assert (socket_re);
      g_assert_no_error (err);

      if (g_regex_match (socket_re, line, 0, &socket_mi)) {
        watch->socket = g_match_info_fetch (socket_mi, 1);
        g_assert (watch->socket);
        g_debug ("Found socket %s", watch->socket);
        finished = TRUE;
        /* we're done */
        g_main_loop_quit (watch->loop);
      }
    }
    break;
    case G_IO_STATUS_EOF:
      finished = TRUE;
      break;
    case G_IO_STATUS_ERROR:
      finished = TRUE;
      g_warning ("Error reading from phoc: %s\n", err->message);
      return FALSE;
    case G_IO_STATUS_AGAIN:
    default:
      break;
    }
  } else if (condition & G_IO_HUP) {
    finished = TRUE;
  }

  return !finished;
}

static gboolean
on_phoc_startup_timeout (gpointer unused)
{
  g_assert_not_reached ();
  return G_SOURCE_REMOVE;
}

static void
on_phoc_exit (GPid     pid,
              int      status,
              gpointer user_data)
{
  g_autoptr (GError) err = NULL;
  if (status == 0 || status == SIGTERM)
    return;
  g_spawn_check_exit_status (status, &err);
  g_assert_no_error (err);
}


/*
 * Get a #PhoshMonitor for layer-surface tests
 */
PhoshMonitor*
phosh_test_get_monitor(void)
{
  PhoshWayland *wl = phosh_wayland_get_default ();
  GHashTable *outputs;
  GHashTableIter iter;
  gpointer wl_output;
  PhoshMonitor *monitor;

  g_assert (PHOSH_IS_WAYLAND (wl));
  outputs = phosh_wayland_get_wl_outputs (wl);

  g_hash_table_iter_init (&iter, outputs);
  g_hash_table_iter_next (&iter, NULL, &wl_output);

  monitor = phosh_monitor_new_from_wl_output (wl_output);
  g_assert (PHOSH_IS_MONITOR (monitor));
  return monitor;
}


PhoshTestCompositorState *
phosh_test_compositor_new (void)
{
  g_autoptr (GError) err = NULL;
  g_autoptr (GPtrArray) argv = NULL;
  g_autofree char *run_arg = NULL;
  g_autoptr (GIOChannel) channel = NULL;
  g_autoptr (GMainLoop) mainloop = NULL;
  PhoshTestCompositorState *state;
  GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD;
  GHashTable *outputs;
  GHashTableIter iter;
  const char *comp;
  gboolean ret;
  int outfd;
  PhocOutputWatch watch;

  comp = g_getenv ("PHOSH_PHOC_BINARY");
  if (!comp) {
    flags |= G_SPAWN_SEARCH_PATH;
    comp = "phoc";
  }

  state = g_new0 (PhoshTestCompositorState, 1);

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, (char*)comp);
  g_ptr_array_add (argv, NULL);

  g_setenv ("WLR_BACKENDS", "headless", TRUE);
  ret = g_spawn_async_with_pipes (
    NULL, /* cwd */
    (char **)argv->pdata, NULL,
    flags,
    NULL, NULL,
    &state->pid,
    NULL, &outfd, NULL,
    &err);

  g_assert_no_error (err);
  g_assert_true (ret);

  g_debug ("Spawned compositor %s with pid %d", comp, state->pid);

  mainloop = g_main_loop_new (NULL, FALSE);
  watch.loop = mainloop;
  watch.socket = NULL;
  channel = g_io_channel_unix_new (outfd);
  g_io_channel_set_close_on_unref (channel, TRUE);
  g_io_channel_set_flags (channel,
                          g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
                          NULL);
  g_io_add_watch (channel,
                  G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                  (GIOFunc)phoc_stdout_watch,
                  &watch);
  g_child_watch_add (state->pid, on_phoc_exit, NULL);

  g_timeout_add_seconds (10, on_phoc_startup_timeout, NULL);
  g_main_loop_run (mainloop);

  /* I/O watch in main should have gotten the socket name */
  g_assert (watch.socket);
  g_debug ("Found wayland display: '%s'", watch.socket);
  g_setenv ("WAYLAND_DISPLAY", watch.socket, TRUE);

  /* Set up wayland protocol */
  gdk_set_allowed_backends ("wayland");
  /*
   * Don't let GDK decide whether to open a display, we want the one
   * from the freshly spawned compositor
   */
  state->gdk_display = gdk_display_open (watch.socket);
  g_free (watch.socket);
  state->wl = phosh_wayland_get_default ();

  /* Get us the first output just so it's simpler to use */
  outputs = phosh_wayland_get_wl_outputs (state->wl);
  g_hash_table_iter_init (&iter, outputs);
  g_hash_table_iter_next (&iter, NULL, (gpointer*)&state->output);
  g_assert_nonnull (state->output);

  return state;
}

void
phosh_test_compositor_free (PhoshTestCompositorState *state)
{
  if (!state)
    return;

  gdk_display_close (state->gdk_display);
  kill (state->pid, SIGTERM);
  g_spawn_close_pid (state->pid);

#if GLIB_CHECK_VERSION(2,62,0)
  g_assert_finalize_object (state->wl);
#else
  g_object_unref (state->wl);
#endif

  g_clear_pointer (&state, g_free);
}
