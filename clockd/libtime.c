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
  - The Daemon

  \b libtime
  - API library (see libtime.c and libtime.h)

  \b csd
  - Cellular service daemon
  - Sends "network time changed" signal when operator time/timezone has
    been received from vellnet (when attached to network)
 */

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include "libtime.h"
#include "codec.h"
#include <dbus/dbus.h>
#include "clock_dbus.h"
#include <pthread.h>
#include <semaphore.h>

/**
 * @brief Implementation of libtime.
 *
 * @file  libtime.c
 *
 * This is the implementation of libtime (see libtime.h) <br>
 * Include also file <time.h>
 *
 * @copyright GNU GPLv2 or later
 */

static bool s_inited = false;
static bool s_autosync_enabled = false;
static bool s_operator_time_available = false;
static sem_t sem_time;
static DBusConnection *clockd_conn = NULL;
static char s_tz[CLOCKD_TZ_SIZE] = {0, };
static char s_default_tz[CLOCKD_TZ_SIZE] = {0, };
static char s_time_format[CLOCKD_GET_TIMEFMT_SIZE] = {0, };

#define TIME_TRY_INIT_SYNC(__ret__) \
do { \
  sem_wait(&sem_time); \
 \
  if (!s_inited) \
  { \
    if (get_synced()) \
    { \
      TIME_EXIT_SYNC; \
      return __ret__; \
    } \
 \
    s_inited = true; \
  } \
} while(0)

#define TIME_EXIT_SYNC sem_post(&sem_time)
#define TIME_INIT_ERROR "libtime_init() error\n"

/**
 * Initialize libtime shlib:
 * - semaphore
 * - cached data
 */
__attribute__((constructor)) static void
libtime_init()
{
  if (sem_init(&sem_time, 0, 1))
    if (write(STDERR_FILENO, TIME_INIT_ERROR, strlen(TIME_INIT_ERROR))) {}
}

/**
 * Deinitialize libtime shlib:
 * - semaphore
 */
__attribute__((destructor)) static void
libtime_fini()
{
  if (clockd_conn)
  {
    dbus_connection_close(clockd_conn);
    dbus_connection_unref(clockd_conn);
    clockd_conn = NULL;
  }

  sem_destroy(&sem_time);
}

/**
 * Create a new D-Bus request.
 *
 * @param method     D-Bus method
 * @param dbus_type  Vararg list of type/value pairs, last shall be
 *                   DBUS_TYPE_INVALID
 *
 * @return  Pointer to created DBus message, NULL if error
 */
static DBusMessage *
client_new_req(char *method, int first_arg_type, ...)
{
  DBusMessage *req;
  va_list va;

  va_start(va, first_arg_type);
  req = dbus_message_new_method_call(CLOCKD_SERVICE, CLOCKD_PATH,
                                     CLOCKD_INTERFACE, method);

  if (req)
  {
    if ( !dbus_message_append_args_valist(req, first_arg_type, va) )
      fprintf(stderr, "FAILED: %s\n", "dbus_message_append_args_valist");
  }
  else
    fprintf(stderr, "FAILED: %s\n", "dbus_message_new_method_call");

  return req;
}

static void
time_dbus_connection_close()
{
  if (clockd_conn)
  {
    dbus_connection_close(clockd_conn);
    dbus_connection_unref(clockd_conn);
    clockd_conn = NULL;
  }
}

/**
 * Send method call and wait for response
 * @param msg  D-Bus message (from client_new_req)
 * @return     Pointer to received DBus response message, NULL if error
 */
static DBusMessage *
client_get_rsp(DBusMessage *msg)
{
  DBusMessage *rsp = NULL;
  int i = 1;
  DBusError error = DBUS_ERROR_INIT;

  do
  {
    if (clockd_conn ||
        (clockd_conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error)))
    {
      rsp = dbus_connection_send_with_reply_and_block(clockd_conn, msg, -1,
                                                      &error);
      if (rsp)
        break;

      fprintf(stderr, "FAILED: %s\n",
              "dbus_connection_send_with_reply_and_block");
      fprintf(stderr, "->\t%s: %s\n", error.name, error.message);
    }
    else
    {
      fprintf(stderr, "FAILED: %s\n", "dbus_bus_get_private");
      fprintf(stderr, "->\t%s: %s\n", error.name, error.message);
    }

    time_dbus_connection_close();
  }
  while (i--);

  dbus_error_free(&error);

  return rsp;
}

/**
 * Execute 'set_time' method call over D-Bus
 * @param tick  Current time (from Epoch)
 * @return      1 if OK, 0 if fails
 */
static int
client_set_time(time_t tick)
{
  DBusMessage *msg;
  dbus_int32_t db_time = tick;
  dbus_bool_t result = FALSE;

  msg = client_new_req(CLOCKD_SET_TIME, DBUS_TYPE_INT32, &db_time,
                       DBUS_TYPE_INVALID);
  if (msg)
  {
    DBusMessage *rsp = client_get_rsp(msg);

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;
      dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &result,
                            DBUS_TYPE_INVALID);
      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }

    dbus_message_unref(msg);
  }

  return result;
}

/**
 * Execute 'activate_net_time' method call over D-Bus
 * @return  1 if OK, 0 if fails
 */
static int
client_activate_net_time(void)
{
  DBusMessage *req;
  dbus_bool_t result = FALSE;

  req = client_new_req(CLOCKD_ACTIVATE_NET_TIME, DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;
      dbus_message_get_args(rsp, &error,
                            DBUS_TYPE_BOOLEAN, &result,
                            DBUS_TYPE_INVALID);
      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  return result;
}

/**
 * Execute 'get_tz' method call over D-Bus
 *
 * @return  Pointer to locally stored timezone, NULL if fails.
 *          If success, activates the current time zone
 */
static const char *
client_get_tz()
{
  DBusError error = DBUS_ERROR_INIT;
  DBusMessage *req = client_new_req(CLOCKD_GET_TZ, DBUS_TYPE_INVALID);
  const char *rv = NULL;

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      char *s = NULL;

      if (dbus_message_get_args(rsp, &error, DBUS_TYPE_STRING, &s,
                                DBUS_TYPE_INVALID) && s)
      {
        snprintf(s_tz, sizeof(s_tz), "%s", s);

        if (s_tz[0] == '/')
          s_tz[0] = ':';

        rv = s_tz;

        if (*s)
        {
          setenv("TZ", s_tz, 1);
          tzset();
        }
      }

      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  dbus_error_free(&error);

  return rv;
}

/**
 * Execute 'set_tz' method call over D-Bus
 * @param tz  Timezone
 * @return    1 if OK, 0 if fails. If success, sets timezone also locally.
 */
static int
client_set_tz(const char *tz)
{
  DBusMessage *req;
  dbus_bool_t result = false;

  req = client_new_req(CLOCKD_SET_TZ, DBUS_TYPE_STRING, &tz, 0);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;

      dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &result,
                            DBUS_TYPE_INVALID);

      if (result)
      {
        snprintf(s_tz, sizeof(s_tz), "%s", tz);
        setenv("TZ", tz, 1);
        tzset();
      }

      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  return result;
}

/**
 * Execute 'net_time_changed' method call over D-Bus
 *
 * @param t    Pointer to (tick) to store time
 * @param s    Pointer to buffer to store timezone
 * @param max  Size of 's'
 *
 * @return     Number of characters wrote to 's'. -1 if network time has not
 *             been changed. Does not write more than size bytes (including
 *             the trailing "\0"). If the output was truncated due to this
 *             limit then the return value is the number of characters (not
 *             including the trailing "\0") which would have been written to
 *             the final string if enough space had been available. Thus, a
 *             return value of size or more means that the output was
 *             truncated.
 */
static int
client_get_net_time(time_t *t, char *s, size_t max)
{
  int rv = -1;
  DBusMessage *req;

  *t = 0;

  req = client_new_req(CLOCKD_NET_TIME_CHANGED, DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);
    dbus_int32_t tick;
    char *tz = NULL;

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;
      if (dbus_message_get_args(rsp, &error,
                                DBUS_TYPE_INT32, &tick,
                                DBUS_TYPE_STRING, &tz,
                                DBUS_TYPE_INVALID) && tick)
      {
        *t = tick;

        if (tz)
        {
          snprintf(s, max, "%s", tz);
          rv = strlen(tz);
        }
        else
          rv = 0;
      }

      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }
    dbus_message_unref(req);
  }

  return rv;
}

/**
 * Execute 'get_timefmt' method call over D-Bus
 * @return  Pointer to locally stored time format, NULL if error
 */
static const char *
client_get_time_format()
{
  DBusError error = DBUS_ERROR_INIT;
  DBusMessage *req = client_new_req(CLOCKD_GET_TIMEFMT, DBUS_TYPE_INVALID);
  const char *rv = NULL;

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      char *s = NULL;

      if (dbus_message_get_args(rsp, &error, DBUS_TYPE_STRING, &s,
                                DBUS_TYPE_INVALID) && s && *s)
      {
        snprintf(s_time_format, sizeof(s_time_format), "%s", s);
        rv = s_time_format;
      }

      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  dbus_error_free(&error);

  return rv;
}

/**
 * Execute 'set_timefmt' method call over D-Bus
 * @param fmt  Time formatter string
 * @return     1 if OK, 0 if fails
 */
static int
client_set_time_format(const char *fmt)
{
  DBusMessage *req;
  dbus_bool_t result = FALSE;

  req = client_new_req(CLOCKD_SET_TIMEFMT, DBUS_TYPE_STRING, &fmt,
                       DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;
      dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &result, 0);
      if (result)
        snprintf(s_time_format, sizeof(s_time_format), "%s", fmt);

      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  return result;
}

/**
 * Execute 'have_opertime' method call over D-Bus
 *
 * @return  1 if network time service is available (in the
 *          hardware/configuration), 0 if not
 */
static int
client_is_operator_time_accessible()
{
  DBusError error = DBUS_ERROR_INIT;
  DBusMessage *req = client_new_req(CLOCKD_HAVE_OPERTIME, DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      dbus_bool_t b = FALSE;

      if (dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &b,
                                DBUS_TYPE_INVALID))
      {
        s_operator_time_available = (b != FALSE);
      }

      dbus_message_unref(rsp);
    }
    dbus_message_unref(req);
  }

  dbus_error_free(&error);

  return s_operator_time_available;
}

/**
 * Execute 'get_autosync' method call over D-Bus
 * @return  >0 if autosync is enabled, 0 if not
 */
static int
client_get_autosync()
{
  DBusError error = DBUS_ERROR_INIT;
  DBusMessage *req = client_new_req(CLOCKD_GET_AUTOSYNC, DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      dbus_bool_t b = FALSE;

      if (dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &b,
                                DBUS_TYPE_INVALID))
      {
        s_autosync_enabled = (b != FALSE);
      }

      dbus_message_unref(rsp);
    }
    dbus_message_unref(req);
  }

  dbus_error_free(&error);

  return s_autosync_enabled;
}

/**
 * Execute 'set_autosync' method call over D-Bus
 * @param enable  >0 if autosync is enabled, 0 if not
 * @return  1 if OK, 0 if fails
 */
static int
client_set_autosync(int enable)
{
  DBusMessage *req;
  dbus_bool_t result = FALSE;
  dbus_bool_t db_enable = !!enable;

  req = client_new_req(CLOCKD_SET_AUTOSYNC, DBUS_TYPE_BOOLEAN, &db_enable,
                       DBUS_TYPE_INVALID);

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      DBusError error = DBUS_ERROR_INIT;

      dbus_message_get_args(rsp, &error, DBUS_TYPE_BOOLEAN, &result,
                            DBUS_TYPE_INVALID);

      if (result)
        s_autosync_enabled = enable;

      dbus_error_free(&error);
      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  return result;
}

/**
 * Execute 'get_default_tz' method call over D-Bus
 * @return  Pointer to locally stored tz, NULL if error
 */
static const char*
client_get_default_tz()
{
  DBusMessage *req = client_new_req(CLOCKD_GET_DEFAULT_TZ, DBUS_TYPE_INVALID);
  DBusError error = DBUS_ERROR_INIT;
  const char *rv = NULL;

  if (req)
  {
    DBusMessage *rsp = client_get_rsp(req);

    if (rsp)
    {
      char *s = NULL;

      if (dbus_message_get_args(rsp, &error, DBUS_TYPE_STRING, &s,
                                DBUS_TYPE_INVALID) && s && *s)
      {
        snprintf(s_default_tz, sizeof(s_default_tz), "%s", s);
        rv = s_default_tz;
      }

      dbus_message_unref(rsp);
    }

    dbus_message_unref(req);
  }

  dbus_error_free(&error);

  return rv;
}

/**
 * Sync local (cached) data with clockd.
 * @return  0 if OK, -1 if fails
 */
static int
get_synced()
{
  int rv = (client_get_tz() == NULL);

  client_get_time_format();
  client_is_operator_time_accessible();
  client_get_autosync();
  client_get_default_tz();

  return rv;
}

/**
 * Function to call when "time changed" indication (see
 * osso_time_set_notification_cb) has been received by the application.<br>
 * If the application does not wish to link with libosso, then it can listen
 * to the CLOCKD_TIME_CHANGED D-Bus signal, that clockd sends also.
 *
 * The time changed indication is broadcasted to all libtime users when:
 *
 * - time is changed (time_set_time or by automatic network time change)
 * - time zone is changed (time_set_timezone or by automatic network time
 *   change)
 * - time formatter is changed (time_set_time_format)
 * - automatic network time setting is changed (time_set_autosync)
 *
 * @return  0 if OK, -1 if fails
 */
int
time_get_synced(void)
{
  int synced;

  sem_wait(&sem_time);
  synced = get_synced();
  sem_post(&sem_time);

  if (!synced)
    s_inited = true;

  return synced;
}

/**
 * Get current time - see time()
 */
time_t
time_get_time(void)
{
  return time(NULL);
}

/**
 * Set current system and RTC time
 * @param tick  Time since Epoch
 * @return      0 if OK, -1 if fails
 */
int
time_set_time(time_t tick)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_set_time(tick) ? 0 : -1;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get the network time (in case that autosync is not enabled and a network
 * time change indication has received)
 *
 * @param tick  Supplied buffer to store network time (ticks since Epoch)
 * @param s     Supplied buffer to store network timezone
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'. -1 if network time has not
 *              been changed. Does not write more than size bytes (including
 *              the trailing "\0"). If the output was truncated due to this
 *              limit then the return value is the number of characters (not
 *              including the trailing "\0") which would have been written to
 *              the final string if enough space had been available. Thus,
 *              a return value of size or more means that the output was
 *              truncated.
 */
int
time_get_net_time(time_t *tick, char *s, size_t max)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_get_net_time(tick, s, max);

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Deprecated, see time_get_net_time
 */
int
time_is_net_time_changed(time_t *tick, char *s, size_t max)
{
  return time_get_net_time(tick, s, max);
}

/**
 * Set current system and RTC time according to operator network time in case
 * time_is_net_time_changed() indicates change
 *
 * @return  0 if OK, -1 if fails (for example if the network time has not changed)
 */
int
time_activate_net_time(void)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_activate_net_time() ? 0 : -1;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Make time_t from struct tm. Like mktime() but timezone can be given.
 *
 * @param tm  See mktime
 * @param tz  Time zone variable. All formats that glibc supports can be
 *            given. NULL if current zone is used.<br>
 *            See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return    Time since Epoch (0) if error
 */
time_t
time_mktime(struct tm *tm, const char *tz)
{
  time_t rv;

  TIME_TRY_INIT_SYNC(0);

  if (tz)
  {
    setenv("TZ", tz, 1);
    tzset();
  }

  rv = mktime(tm);

  if (tz)
  {
    setenv("TZ", s_tz, 1);
    tzset();
  }

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get current time zone. May return empty string if time zone has not been
 * set.<br>
 * Example output:<br>
 * :US/Central <br>
 * GMT+5:00GMT+4:00,0,365 <br>
 * The latter format is returned when network has indicated time zone change
 * (the actual location is not known)
 *
 * @param s    Supplied buffer to store timezone
 * @param max  Size of 's', including terminating NUL
 *
 * @return     Number of characters wrote to 's'. Does not write more than
 *             size bytes (including the trailing "\0"). If the output was
 *             truncated due to this limit then the return value is the
 *             number of characters (not including the trailing "\0") which
 *             would have been written to the final string if enough space
 *             had been available. Thus, a return value of size or more means
 *             that the output was truncated. If an output error is
 *             encountered, a negative value is returned.
 */
int
time_get_timezone(char *s, size_t max)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = snprintf(s, max, "%s", s_tz);

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get current time zone name (like "EET")
 *
 * @param s    Supplied buffer to store tzname
 * @param max  Size of 's', including terminating NUL
 *
 * @return     Number of characters wrote to 's'. Does not write more than
 *             size bytes (including the trailing "\0"). If the output was
 *             truncated due to this limit then the return value is the
 *             number of characters (not including the trailing "\0") which
 *             would have been written to the final string if enough space
 *             had been available. Thus, a return value of size or more means
 *             that the output was truncated. If an output error is
 *             encountered, a negative value is returned.
 */
int
time_get_tzname(char *s, size_t max)
{
  int rv = -1;
  struct tm tp;
  time_t t;

  TIME_TRY_INIT_SYNC(-1);

  memset(&tp, 0, sizeof(tp));
  t = time(0);

  if (localtime_r(&t, &tp))
    rv = snprintf(s, max, "%s", tzname[tp.tm_isdst ? 1 : 0]);

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * See difftime()
 */
double
time_diff(time_t t1, time_t t2)
{
  return difftime(t1, t2);
}

/**
 * Set current time zone ("TZ")
 *
 * @param tz  Time zone variable. All formats that glibc supports can be
 *            given.<br>
 *            See
 *            http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html<br>
 *            It is, however recommended to use /usr/share/zoneinfo -method,
 *            for example:<br>
 *             :Europe/Rome<br>
 *            and not like this:<br>
 *             EST+5EDT,M4.1.0/2,M10.5.0/2
 *
 * @return    0 if OK, -1 if fails
 */
int
time_set_timezone(const char *tz)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_set_tz(tz) ? 0 : -1;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get current time in UTC. Inspired by time() and gmtime_r()
 * @param tm  Supplied buffer to store result
 * @return    0 if OK, -1 if error
 */
int
time_get_utc(struct tm *tm)
{
  struct tm *tp;
  time_t timer;

  TIME_TRY_INIT_SYNC(-1);

  timer = time(0);
  tp = gmtime_r(&timer, tm);

  TIME_EXIT_SYNC;

  return tp ? 0 : -1;
}

/**
 * Get time in UTC.
 *
 * @param tick  Time
 * @param tm    Supplied buffer to store result
 *
 * @return      0 if OK, -1 if error
 */
int
time_get_utc_ex(time_t tick, struct tm *tm)
{
  struct tm *tp;

  TIME_TRY_INIT_SYNC(-1);

  tp = gmtime_r(&tick, tm);

  TIME_EXIT_SYNC;

  return tp ? 0 : -1;
}

/**
 * Get current local time. Inspired by time() and localtime_r().
 * @param tm  Supplied buffer to store tm
 * @return    0 if OK, -1 if error
 */
int
time_get_local(struct tm *tm)
{
  struct tm *tp;
  time_t timer;

  TIME_TRY_INIT_SYNC(-1);

  timer = time(0);
  tp = localtime_r(&timer, tm);

  TIME_EXIT_SYNC;

  return tp ? 0 : -1;
}

/**
 * Get local time.
 *
 * @param tick  Time
 * @param tm    Supplied buffer to store tm
 *
 * @return      0 if OK, -1 if error
 */
int
time_get_local_ex(time_t tick, struct tm *tm)
{
  struct tm *tp;

  TIME_TRY_INIT_SYNC(-1);

  tp = localtime_r(&tick, tm);

  TIME_EXIT_SYNC;

  return tp ? 0 : -1;
}

/**
 * Get local time in given zone. Inspired by setting TZ temporarily and
 * localtime_r().
 *
 * @param tick  Time since Epoch
 * @param tz    Time zone variable. All formats that glibc supports can be given.<br>
 *              See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 * @param tm    Supplied buffer to store tm
 *
 * @return      0 if OK, -1 if error
 */
int
time_get_remote(time_t tick, const char *tz, struct tm *tm)
{
  int rv = -1;

  TIME_TRY_INIT_SYNC(-1);

  setenv("TZ", tz, 1);
  tzset();

  if (localtime_r(&tick, tm))
    rv = 0;

  setenv("TZ", s_tz, 1);
  tzset();

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get the default time zone. Empty string is returned if no default zone has
 * been defined.
 *
 * @param s    Supplied buffer to store the zone
 * @param max  Size of 's', including terminating NUL
 *
 * @return     Number of characters wrote to 's'. Does not write more than
 *             size bytes (including the trailing "\0"). If the output was
 *             truncated due to this limit then the return value is the
 *             number of characters (not including the trailing "\0") which
 *             would have been written to the final string if enough space
 *             had been available. Thus, a return value of size or more means
 *             that the output was truncated. If an output error is
 *             encountered, a negative value is returned.
 */
int
time_get_default_timezone(char *s, size_t max)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = snprintf(s, max, "%s", s_default_tz);

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get current time string formatter. Inspired by strftime().
 *
 * @param s    Supplied buffer to store formatter
 * @param max  Size of 's', including terminating NUL
 *
 * @return     Number of characters wrote to 's'. Does not write more than
 *             size bytes (including the trailing "\0"). If the output was
 *             truncated due to this limit then the return value is the
 *             number of characters (not including the trailing "\0") which
 *             would have been written to the final string if enough space
 *             had been available. Thus, a return value of size or more means
 *             that the output was truncated. If an output error is
 *             encountered, a negative value is returned.
 */
int
time_get_time_format(char *s, size_t max)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = snprintf(s, max, "%s", s_time_format);

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Set current time string formatter. Inspired by strftime()
 * @param fmt  Formatter string
 * @return     0 if OK, -1 if fails
 */
int
time_set_time_format(const char *fmt)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_set_time_format(fmt) ? 0 : -1;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Format given time to string. Inspired by strftime() and localtime_r()
 *
 * @param tm   Time
 * @param fmt  Formatter, see strftime and time_set_time_format and
 *             time_get_time_format, NULL if active formatter is used.
 * @param s    Supplied buffer to store result
 * @param max  Size of 's', including terminating NUL
 *
 * @return     See strftime()
 */
int
time_format_time(const struct tm *tm, const char *fmt, char *s, size_t max)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  if (!fmt)
    fmt = s_time_format;

  rv = strftime(s, max, fmt, tm);

  TIME_EXIT_SYNC;

  return rv;
}

static int
get_utc_offset(time_t tick)
{
  struct tm *lt;
  struct tm tp;
  int rv;

  memset(&tp, 0, sizeof(tp));
  lt = localtime_r(&tick, &tp);
  rv = -1;

  if (lt)
    rv = -tp.tm_gmtoff;

  return rv;
}

/**
 * Get utc offset (secs west of GMT) in the named TZ. The current daylight
 * saving time offset is included.
 *
 * @param tz  Time zone, all formats that glibc supports can be given. NULL
 *            to use current tz.<br>
 *            See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return    Secs west of GMT. or -1 in case of error
 */
int
time_get_utc_offset(const char *tz)
{
  int rv;
  time_t tick;

  TIME_TRY_INIT_SYNC(-1);

  if (tz)
  {
    setenv("TZ", tz, 1);
    tzset();
  }

  tick = time(0);
  rv = get_utc_offset(tick);

  if (tz)
  {
    setenv("TZ", s_tz, 1);
    tzset();
  }

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Return if daylight-saving-time is in use in given time.
 *
 * @param tick  Time since Epoch
 * @param tz    Time zone, all formats that glibc supports can be given. NULL
 *              to use current tz.<br>
 *              See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return      Non-zero if daylight savings time is in effect, zero if not,
 *              -1 if error
 */
int
time_get_dst_usage(time_t tick, const char *tz)
{
  int rv = -1;
  int timediff;
  int gmt_off;
  struct tm tp;

  TIME_TRY_INIT_SYNC(-1);

  if (tz)
  {
    setenv("TZ", tz, 1);
    tzset();
  }

  memset(&tp, 0, sizeof(tp));

  if (localtime_r(&tick, &tp) && tp.tm_isdst > 0)
  {
    time_t t1, t2;

    t1 = mktime(&tp);
    tp.tm_isdst = 0;
    t2 = mktime(&tp);
    timediff = t2 - t1;

    if (!timediff)
    {
      gmt_off = tp.tm_gmtoff;
      tp.tm_mday = 31;
      tp.tm_mon = 11;
      t2 = mktime(&tp);
      memset(&tp, 0, sizeof(tp));
      timediff = gmt_off;

      if (localtime_r(&t2, &tp))
        timediff = gmt_off - tp.tm_gmtoff;
    }

    rv = !!timediff;
  }

  if (tz)
  {
    setenv("TZ", s_tz, 1);
    tzset();
  }

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Enable or disable automatic time settings based on cellular network time.
 * @param enable  Non-zero to enable, zero to disable
 * @return  0 if OK, -1 if error
 */
int
time_set_autosync(int enable)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = client_set_autosync(enable) ? 0 : -1;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get the state of automatic time settings based on cellular network time.
 * @return  Non-zero if enabled, zero if disabled, -1 if error
 */
int
time_get_autosync(void)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = s_autosync_enabled;

  TIME_EXIT_SYNC;

  return rv;
}

/**
 * Get info if the device supports network time updates (i.e. has CellMo).
 * @return  Non-zero if accessible, 0 if not, -1 if error
 */
int time_is_operator_time_accessible(void)
{
  int rv;

  TIME_TRY_INIT_SYNC(-1);

  rv = s_operator_time_available;

  TIME_EXIT_SYNC;

  return rv;
}

static const char *
fix_tz(const char *tz, char *tz_fixed)
{
  if (tz[0] && isalpha(tz[0]) && tz[1] && isalpha(tz[1]))
  {
    if (tz[2] && (tz[2] == '-' || tz[2] == '+') && isdigit(tz[3]))
    {
      int offset = atoi(tz + 2);

      if (offset)
      {
        setenv("TZ", tz, 1);
        tzset();

        if (timezone != offset)
        {
          snprintf(tz_fixed, 24u, "GMT%s%d", offset < 0 ? "+" : "-",
                   abs(offset));
          tz = tz_fixed;
        }
      }
    }
  }

  return tz;
}

/**
 * Get time difference between two timezones
 *
 * @param tick  Time since Epoch
 * @param tz1   Timezone 1
 * @param tz2   Timezone 2
 *
 * @return      Local time in tz1 - local time in tz2
 */
int
time_get_time_diff(time_t tick, const char *tz1, const char *tz2)
{
  const char *tz1_fixed, *tz2_fixed;
  time_t t1, t2;
  struct tm tp;
  char tz1_buf[24], tz2_buf[24];

  TIME_TRY_INIT_SYNC(0);

  tz1_fixed = fix_tz(tz1, tz1_buf);
  tz2_fixed = fix_tz(tz2, tz2_buf);

  setenv("TZ", tz1_fixed, 1);
  tzset();
  localtime_r(&tick, &tp);
  t1 = mktime(&tp) - get_utc_offset(tick);

  setenv("TZ", tz2_fixed, 1);
  tzset();
  localtime_r(&tick, &tp);
  t2 = mktime(&tp) - get_utc_offset(tick);

  setenv("TZ", s_tz, 1);
  tzset();

  TIME_EXIT_SYNC;

  return t1 - t2;
}
