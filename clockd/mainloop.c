/**
 * @brief clockd mainloop
 *
 * @file  mainloop.c
 * This initializes/deinitializes clockd:
 *
 * @copyright GNU GPLv2 or later
 */

#include <glib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <dbus/dbus.h>

#include "mainloop.h"
#include "logging.h"
#include "server.h"
#include "sighnd.h"

static GMainLoop *mainloop = NULL;

/**
 * This is clockd mainloop.
 *
 * Args:<br>
 * -d enable debug mode
 *
 * - init signals
 * - inits dbus (server)
 * - starts g_mainloop
 *
 * @param argc  argc
 * @param argv  argv
 *
 * @return  0 if exited normally, non-zero if exit with failure.
 */
int
mainloop_run(int argc, char **argv)
{
  int i;
  int rv = -1;

  for (i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "-d"))
      clockd_debug_mode = 1;
  }

  signal(SIGPIPE, SIG_IGN);
  sighnd_setup();

  if (server_init() != -1)
  {
    mainloop = g_main_loop_new(0, 0);

    DO_LOG(LOG_DEBUG, "enter main loop");

    g_main_loop_run(mainloop);
    rv = 0;

    DO_LOG(LOG_DEBUG, "leave main loop");
  }

  if (mainloop)
  {
    g_main_loop_unref(mainloop);
    mainloop = 0;
  }

  server_quit();

  DO_LOG(LOG_DEBUG, "exit with code %d", rv);

  return rv;
}

/**
 * Quit the mainloop.
 * @param force  If non-zero and no mainloop, exit the process with failure
 * @return       1 if success, 0 if fails (no mainloop)
 */
int
mainloop_stop(int force)
{
  int rv;

  if (mainloop)
  {
    g_main_loop_quit(mainloop);
    rv = 1;
  }
  else
  {
    DO_LOG(LOG_WARNING, "mainloop_stop: no main loop");
    rv = 0;

    if (force)
      exit(1);
  }

  return rv;
}
