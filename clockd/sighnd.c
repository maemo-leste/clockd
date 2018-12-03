/**
 * @brief init clockd signals
 *
 * @file  sighnd.c
 * This initializes clockd signal handler.
 *
 * @copyright GNU GPLv2 or later
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "logging.h"
#include "mainloop.h"
#include "sighnd.h"

static int terminating = false;

static void sighnd_terminate(void);
static void sighnd_handler(int sig);

static const int signals[] =
{
  SIGHUP,
  SIGINT,
  SIGQUIT,
  SIGUSR1,
  SIGTERM,
  -1
};

/**
 * Setup signals as follows:
 * - SIGUP: do nothing
 * - SIGUSR1: toggle debug mode
 * - SIGINT, SIGQUIT, SIGTERM: terminate clockd
 */
void
sighnd_setup(void)
{
  int i;

  for ( i = 0; ; ++i )
  {
    if (signals[i] == -1)
      break;

    signal(signals[i], sighnd_handler);
  }
}

/**
 * Internal signal helper function which is called when process is signalled.
 * This is needed just for breaking away from while loop in main() function.
 */
static void
sighnd_handler(int sig)
{
  DO_LOG(LOG_DEBUG, "got signal [%d] %s", sig, strsignal(sig));

  switch (sig)
  {
    case SIGUSR1:
    {
      clockd_debug_mode = !clockd_debug_mode;
      DO_LOG(LOG_INFO, "%s debug mode",
             clockd_debug_mode ? "enabled" : "disabled");
      break;
    }
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
    {
      sighnd_terminate();
    }
    default: /* fallthrough */
      break;
  }
}

/**
 * Handle clockd terminate signal.
 * @todo  Is it safe to call g_main_loop_quit() from signal handler?
 */
static void
sighnd_terminate(void)
{
  DO_LOG(LOG_DEBUG, "sighnd_terminate");

  if (++terminating != 1)
  {
    DO_LOG(LOG_INFO, "forced shutdown");
    exit(1);
  }

  if (!mainloop_stop(0))
  {
    DO_LOG(LOG_INFO, "exit");
    exit(1);
  }
}
