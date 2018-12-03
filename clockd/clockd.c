/**
 @mainpage

 Introduction
 ------------
  Time management service is a subsystem to provide a common library for
  all time-related (not alarms, though) services.

 Structure
 ---------
  The following ASCII art describes the architecture.

<pre>
+-----+  +-----+  +-----+
|app 1|  |app 2|  |app 3| applications
-------------------------  libtime.h
       | libtime |
       +---------+
         ^ ^ ^
         | | |
   D-Bus | | |
         | | |
         v v v
       +------+  D-Bus +---+
daemon |clockd|<-------|csd|
       +------+        +---+
           |                 userland
-------------------------------------
           |                 kernel
     \- systemtime
     \- RTC (real time clock)
     \- timezone
</pre>

 Components
 ----------
  \b clockd
  - The Daemon (see clockd.c, rclockd.c, mainloop.c and server.c)

  \b libtime
  - API library

  \b csd
  - Cellular service daemon
  - Sends "network time changed" signal when operator time/timezone has
    been received from vellnet (when attached to network)
 */

#include "mainloop.h"

/**
 * @brief clockd main body
 *
 * @file  clockd.c
 * This is the main file for clockd.
 *
 * @copyright GNU GPLv2 or later
 */


/**
 * This is clockd main pgm.
 *
 * @param argc  refer to mainloop_run
 * @param argv  refer to mainloop_run
 *
 * @return      refer to mainloop_run
 */
int
main (int argc, char **argv)
{
  return mainloop_run(argc, argv);
}
