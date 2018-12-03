#ifndef MAINLOOP_H
#define MAINLOOP_H

/**
 * @brief clockd mainloop prototypes
 * @file  mainloop.h
 * @copyright GNU GPLv2 or later
 */


/**
 * Quit the mainloop.
 * @param force  If non-zero and no mainloop, exit the process with failure
 * @return       1 if success, 0 if fails (no mainloop)
 */
int mainloop_stop (int force);

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
 * @return      0 if exited normally, non-zero if exit with failure.
 */
int mainloop_run (int argc, char *argv[]);

#endif  /* MAINLOOP_H */
