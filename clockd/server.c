/**
 * @brief clockd 'server'
 *
 * @file  server.c
 *
 * This is D-Bus server for clockd that communicates with clock/time -aware
 * applications. It also listens for CSD NET_TIME_CHANGE signal and reacts to
 * it accordingly.
 *
 * @copyright GNU GPLv2 or later
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "codec.h"
#include "logging.h"
#include "server.h"
#include "clock_dbus.h"
#include "mcc_tz_utils.h"
#include "internal_time_utils.h"

#define CLOCKD_CONFIGURATION_FILE "/home/user/.clockd.conf"

struct server_callback
{
  const char *member;
  DBusMessage *(*callback)(DBusMessage *method_call);
};

static DBusMessage *server_activate_net_time_cb(DBusMessage *msg);
static DBusMessage *server_is_net_time_changed_cb(DBusMessage *msg);
static DBusMessage *server_set_time_cb(DBusMessage *msg);
static DBusMessage *server_set_tz_cb(DBusMessage *msg);
static DBusMessage *server_set_autosync_cb(DBusMessage *msg);
static DBusMessage *server_set_time_format_cb(DBusMessage *msg);
static DBusMessage *server_get_time_format_cb(DBusMessage *msg);
static DBusMessage *server_get_default_tz_cb(DBusMessage *msg);
static DBusMessage *server_get_tz_cb(DBusMessage *msg);
static DBusMessage *server_get_autosync_cb(DBusMessage *msg);
static DBusMessage *server_have_opertime_cb(DBusMessage *msg);
static DBusMessage *server_get_time_cb(DBusMessage *msg);

static int server_set_time(time_t tick);
static void next_dst_change(time_t tick, bool keep_alarm_timer);
static void server_set_operator_tz_cb(const char *tz);
static int set_network_time(bool save_config);
static int set_net_timezone(const char *tzname);

static bool net_time_setting = false;
static bool autosync = false;
static bool was_dst = false;
static bool net_time_disabled_env = false;
static time_t net_time_changed_time = 0;
static clock_t net_time_last_changed_ticks = 0;
static guint alarm_timer_id;

static char saved_server_opertime_tz[CLOCKD_TZ_SIZE] = {0,};
static char server_tz[CLOCKD_TZ_SIZE] = {0,};
static char restore_tz[CLOCKD_TZ_SIZE] = {0,};
static char default_tz[CLOCKD_TZ_SIZE] = {0,};
static char time_format[CLOCKD_GET_TIMEFMT_SIZE] = {0,};

static DBusConnection *dbus_connection = NULL;
static DBusConnection *dbus_system_connection = NULL;

static const struct server_callback server_callbacks[] =
{
  {CLOCKD_SET_TIME, server_set_time_cb},
  {CLOCKD_GET_TIME, server_get_time_cb},
  {CLOCKD_ACTIVATE_NET_TIME, server_activate_net_time_cb},
  {CLOCKD_NET_TIME_CHANGED, server_is_net_time_changed_cb},
  {CLOCKD_GET_TIMEFMT, server_get_time_format_cb},
  {CLOCKD_SET_TIMEFMT, server_set_time_format_cb},
  {CLOCKD_GET_DEFAULT_TZ, server_get_default_tz_cb},
  {CLOCKD_GET_TZ, server_get_tz_cb},
  {CLOCKD_SET_TZ, server_set_tz_cb},
  {CLOCKD_GET_AUTOSYNC, server_get_autosync_cb},
  {CLOCKD_SET_AUTOSYNC, server_set_autosync_cb},
  {CLOCKD_HAVE_OPERTIME, server_have_opertime_cb},
  {NULL, NULL}
};

/**
 * Create D-Bus method call response message
 *
 * @param msg   request message
 * @param type  vararg type/value attributes
 *
 * @return      NULL if fails, pointer to DBusMessage if OK
 */
static DBusMessage *
server_new_rsp(DBusMessage *msg, int type, ...)
{
  DBusMessage *rsp = NULL;
  va_list va;

  va_start(va, type);
  rsp = dbus_message_new_method_return(msg);

  if (rsp && !dbus_message_append_args_valist(rsp, type, va))
    dbus_message_unref(rsp);

  return rsp;
}

/**
 * Send "change" indication to all interested parties (via libosso).
 * @param t  Current time
 * @return   0 if OK, -1 if error
 */
static int
server_send_time_change_indication(time_t t)
{
  DBusMessage *msg;
  dbus_int64_t dbus64_tick = t;
  dbus_int64_t dbus32_tick = t;
  int rv = -1;

  was_dst = internal_get_dst(t);

  DO_LOG(LOG_DEBUG, "sending OSSO time change notification");

  msg = dbus_message_new_signal("/com/nokia/time", "com.nokia.time", "changed");
  if (msg)
  {
    if (dbus_message_append_args(msg, DBUS_TYPE_INT64, &dbus64_tick,
                                 DBUS_TYPE_INVALID))
    {
      if (dbus_connection_send(dbus_connection, msg, 0))
        DO_LOG(LOG_DEBUG, "sent D-Bus signal %s", "changed");
      else
        DO_LOG(LOG_ERR, "dbus_connection_send failed");
    }
    else
        DO_LOG(LOG_ERR, "dbus_message_append_args failed");

    dbus_message_unref(msg);
  }
  else
    DO_LOG(LOG_ERR, "dbus_message_new_signal failed");

  DO_LOG(LOG_DEBUG, "sending D-Bus time change notification");
  msg = dbus_message_new_signal("/com/nokia/clockd", "com.nokia.clockd",
                                "time_changed");

  if (msg)
  {
    if (dbus_message_append_args(msg, DBUS_TYPE_INT32, &dbus32_tick,
                                 DBUS_TYPE_INVALID))
    {
      if (dbus_connection_send(dbus_connection, msg, 0))
      {
        DO_LOG(LOG_DEBUG, "sent D-Bus signal %s", "time_changed");
        rv = 0;
      }
      else
        DO_LOG(LOG_ERR, "dbus_connection_send failed");
    }
    else
      DO_LOG(LOG_ERR, "dbus_message_append_args failed");

    dbus_message_unref(msg);
  }
  else
    DO_LOG(LOG_ERR, "dbus_message_new_signal failed");

  return rv;
}

/**
 * Save current config to /home/user/.clockd.conf
 * @return  0 if OK, -1 if error
 */
static int save_conf()
{
  FILE *fp;
  char buf[256];
  int rv;

  unlink(CLOCKD_CONFIGURATION_FILE);

  fp = fopen(CLOCKD_CONFIGURATION_FILE, "w");

  if (!fp)
  {
    DO_LOG(LOG_ERR, "failed to open configuration file %s (%s)",
           CLOCKD_CONFIGURATION_FILE, strerror(errno));
    return -1;
  }

  snprintf(buf, sizeof(buf), "/bin/chown user:users %s",
           CLOCKD_CONFIGURATION_FILE);

  if (system(buf) == -1)
    DO_LOG(LOG_ERR, "execute %s failed(%s)", buf, strerror(errno));

  memset(buf, 0, sizeof(buf));

  if (readlink("/etc/localtime", buf, sizeof(buf)) <= 0 ||
      !strcmp(buf, "/etc/localtime"))
  {
    buf[0] = 0;
  }

  rv = fprintf(fp, "time_format=%s\nautosync=%d\nnet_tz=%s\nsystem_tz=%s\n",
               time_format, (int)autosync, server_tz[0] == ':' ? "" : server_tz, buf);
  if (rv == -1)
  {
    DO_LOG(LOG_ERR, "failed to write %s (%s)", CLOCKD_CONFIGURATION_FILE,
           strerror(errno));
  }
  else
  {
    rv = 0;
    DO_LOG(LOG_DEBUG, "configuration file %s saved", CLOCKD_CONFIGURATION_FILE);
  }

  fclose(fp);

  return rv;
}

/**
 * Handle D-Bus net_time_change signal from CSD. If network autosync is
 * enabled, set system/RTC time and sends time change indication with
 * libosso; otherwise just store the data (that can be activated with
 * activate_net_time method later)
 *
 * @param msg  signal message
 *
 * @return     -1 if failed to update time (e.g. NITZ does not supported by
 *             carrier), otherwise 0
 */
static int
handle_csd_net_time_change(DBusMessage *msg)
{
  bool time_changed;
  bool tz_changed;
  int rv = -1;
  /* diff, quarters of an hour, between the local time and GMT, ‑47...+48 */
  int tz_q;
  int is_dst;
  char buf[64] = {0, };
  DBusMessageIter iter;
  struct tm tm_net;
  struct tm tm_utc;
  struct tm tm_old;
  char etc_gmt[] = "Etc/GMT";
  char *tz = NULL;
  time_t time_utc;
  time_t now;
  char *old_tz = NULL;

  memset(&tm_net, 0, sizeof(tm_net));
  dbus_message_iter_init(msg, &iter);

  if (decode_ctm(&iter, &tm_net) == -1)
  {
    DO_LOG(LOG_ERR, "handle_csd_net_time_change(), decode_ctm failed");
    goto out;
  }

  log_tm("NET", &tm_net);
  tz_q = tm_net.tm_yday;

  is_dst = tm_net.tm_isdst;

  now = internal_get_time();

  if (now == -1 || !localtime_r(&now, &tm_old))
    goto out;

  log_tm("OLD", &tm_old);
  memset(&tm_utc, 0, sizeof(tm_utc));
  tm_utc.tm_year = tm_net.tm_year;
  tm_utc.tm_mon = tm_net.tm_mon;
  tm_utc.tm_mday = tm_net.tm_mday;
  tm_utc.tm_hour = tm_net.tm_hour;
  tm_utc.tm_min = tm_net.tm_min;
  tm_utc.tm_sec = tm_net.tm_sec;

  time_utc = internal_mktime_in(&tm_utc, NULL);
  if (time_utc == -1)
  {
    DO_LOG(LOG_ERR, "handle_csd_net_time_change(), time evaluation failed");
    goto out;
  }

  log_tm("UTC", &tm_utc);

  if (!localtime_r(&time_utc, &tm_old))
    goto out;

  log_tm("synced OLD", &tm_old);

  if (tz_q == 100)
  {
    DO_LOG(LOG_DEBUG,
           "Let's keep current tz since network does not send info about it");
    tz = saved_server_opertime_tz;
  }
  else
  {
    mcc_tz_guess_tz_for_country_by_dst_and_offset(
                                   &tm_utc, is_dst, 15 * 60  * tz_q, &tz);
  }

  if (!tz)
  {
    int tz_diff_m = 15 * tz_q;
    char sign = tz_diff_m > 0 ? '-' : '+';
    int abs_m = abs(tz_diff_m);
    int h = abs_m / 60;
    int m = abs_m % 60;

    if (m)
      snprintf(buf, sizeof(buf), ":Etc/GMT%c%d:%d", sign, h, m);
    else
    {
      if (!h)
        snprintf(buf, sizeof(buf), ":Etc/GMT");
      else
        snprintf(buf, sizeof(buf), ":Etc/GMT%c%d", sign, h);
    }

    tz = buf;
    DO_LOG(LOG_WARNING, "TZ guessing failed. \"%s\" TZ will be used", tz);
  }

  internal_tz_set(&old_tz, tz);
  localtime_r(&time_utc, &tm_net);
  internal_tz_res(&old_tz);

  log_tm("NEW", &tm_net);

  DO_LOG(LOG_DEBUG, "timeoff: %+ld", time_utc - now);
  DO_LOG(LOG_DEBUG, "gmtoff: %ld -> %ld", tm_old.tm_gmtoff, tm_net.tm_gmtoff);

  net_time_changed_time = time_utc;
  net_time_last_changed_ticks = times(0);

  if (tz == saved_server_opertime_tz ||
      (((saved_server_opertime_tz[0] && !strstr(saved_server_opertime_tz, etc_gmt)) || strstr(tz, etc_gmt)) &&
      tm_old.tm_gmtoff == tm_net.tm_gmtoff && mcc_tz_is_tz_name_in_country_tz_list(saved_server_opertime_tz)))
  {
    DO_LOG(LOG_DEBUG, "Corner case, saved_server_opertime_tz is kept unchanged");
  }
  else
    snprintf(saved_server_opertime_tz, sizeof(saved_server_opertime_tz),
             tz[0] != ':' ? ":%s" : "%s", tz);
  tz = NULL;

  DO_LOG(LOG_DEBUG,
         "handle_csd_net_time_change: found saved_server_opertime_tz = %s",
         saved_server_opertime_tz);
  DO_LOG(LOG_DEBUG, "handle_csd_net_time_change: current server_tz = %s",
         server_tz);

  time_changed = now != time_utc;
  tz_changed = saved_server_opertime_tz[0] &&
      (internal_tz_cmp(server_tz, saved_server_opertime_tz) ||
       !mcc_tz_is_tz_name_in_country_tz_list(server_tz));

  if (time_changed && autosync && server_set_time(time_utc) == -1)
  {
    DO_LOG(LOG_ERR, "handle_csd_net_time_change(), time setting failed");
    goto out;
  }

  rv = 0;

  if (tz_changed && autosync)
  {
    snprintf(server_tz, sizeof(server_tz), "/%s", &saved_server_opertime_tz[1]);

    if (set_net_timezone(server_tz) == -1)
    {
      DO_LOG(LOG_ERR, "handle_csd_net_time_change(), timezone setting failed");
      rv = -1;
    }

    internal_setenv_tz(server_tz);
  }

  if (time_changed | tz_changed)
    server_send_time_change_indication(time_changed ? internal_get_time() : 0);

  save_conf();
  dump_date(server_tz);

out:
  if (rv)
    DO_LOG(LOG_ERR, "handle_csd_net_time_change() -> FAILED");
  else
    DO_LOG(LOG_DEBUG, "handle_csd_net_time_change() -> OK");

  return rv;
}

/**
 * Handle D-Bus activate_net_time method. Sets system/RTC time and sends time
 * change indication with libosso.
 *
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_activate_net_time_cb(DBusMessage *msg)
{
  dbus_bool_t success = FALSE;

  if (net_time_changed_time && !set_network_time(true))
    success = TRUE;

  return server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus is_net_time_changed method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_is_net_time_changed_cb(DBusMessage *msg)
{
  dbus_int32_t net_time;
  char *tz = saved_server_opertime_tz;

  if (net_time_changed_time)
  {
    struct tms buffer;
    clock_t now = times(&buffer);
    net_time = (now - net_time_last_changed_ticks) / sysconf(_SC_CLK_TCK);
    net_time += net_time_changed_time;
  }
  else
  {
    net_time = 0;
    tz = "";
  }

  return server_new_rsp(msg, DBUS_TYPE_INT32, &net_time, DBUS_TYPE_STRING, &tz,
                        DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus set_time method. Sets system/RTC time and sends time change
 * indication with libosso.
 *
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_set_time_cb(DBusMessage *msg)
{
  DBusMessage *rsp;
  DBusError error = DBUS_ERROR_INIT;
  dbus_int32_t dbus_time = 0;
  dbus_bool_t success = FALSE;

  if (dbus_message_get_args(msg, &error, DBUS_TYPE_INT32, &dbus_time,
                            DBUS_TYPE_INVALID))
  {
    DO_LOG(LOG_DEBUG, "Setting time to %lu", (unsigned long)dbus_time);

    if (!server_set_time(dbus_time))
      success = TRUE;
  }
  else
  {
    DO_LOG(LOG_ERR, "server_set_time_cb() %s : %s : %s",
           dbus_message_get_member(msg), error.name, error.message);
  }

  dbus_error_free(&error);
  rsp = server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);

  if (success)
  {
    dump_date(server_tz);
    save_conf();
    server_send_time_change_indication(dbus_time);
  }

  return rsp;
}

/**
 * Handle D-Bus set_tz method. Sets the time zone and sends time change
 * indication. Saves config to /home/user/.clockd.conf
 *
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_set_tz_cb(DBusMessage *msg)
{
  DBusMessage *rsp;
  DBusError error = DBUS_ERROR_INIT;
  const char *tzname = NULL;
  dbus_bool_t success = FALSE;

  if (dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &tzname,
                            DBUS_TYPE_INVALID))
  {
    DO_LOG(LOG_DEBUG, "Setting time zone to %s", tzname ? tzname : "<null>");

    if (tzname && *tzname && strlen(tzname) < CLOCKD_TZ_SIZE)
    {
      if (*tzname == ':')
      {
        if (!internal_set_tz(tzname))
          success = 1;
      }
      else
        success = !internal_check_timezone(tzname);

      if (success)
      {
        if (internal_setenv_tz(tzname))
          success = FALSE;
        else
        {
          strcpy(server_tz, tzname);
          dump_date(server_tz);
        }
      }
    }
    else
      DO_LOG(LOG_ERR, "invalid time zone '%s", tzname ? tzname : "<null>");

    save_conf();

    if (success)
      next_dst_change(time(0), 0);
  }
  else
  {
    DO_LOG(LOG_ERR, "server_set_tz_cb() %s : %s : %s",
           dbus_message_get_member(msg), error.name, error.message);
  }

  dbus_error_free(&error);

  rsp = server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);

  if ( success )
    server_send_time_change_indication(0);

  return rsp;
}

/**
 * Handle D-Bus set_autosync method. Set autosync on/off and sends time
 * change indication with libosso. Saves config to /home/user/.clockd.conf
 *
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_set_autosync_cb(DBusMessage *msg)
{
  DBusMessage *rsp;
  DBusError error = DBUS_ERROR_INIT;
  dbus_bool_t enabled = FALSE;
  dbus_bool_t success = FALSE;

  if (dbus_message_get_args(msg, &error, DBUS_TYPE_BOOLEAN, &enabled,
                            DBUS_TYPE_INVALID))
  {
    if (enabled && net_time_disabled_env)
      DO_LOG(LOG_ERR, "server_set_autosync_cb(), feature disabled");
    else
    {
      DO_LOG(LOG_DEBUG, "Network time autosync set to '%s' from '%s'",
             enabled ? "on" : "off", autosync ? "on" : "off");

      autosync = enabled;

      if (autosync && net_time_changed_time)
        set_network_time(false);

      mcc_tz_setup_timezone_from_mcc_if_required();

      if (!save_conf())
        success = TRUE;
    }
  }
  else
  {
    DO_LOG(LOG_ERR, "server_set_autosync_cb() %s : %s : %s",
           dbus_message_get_member(msg), error.name, error.message);
  }

  rsp = server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);
  dbus_error_free(&error);

  if (success)
    server_send_time_change_indication(0);

  return rsp;
}

/**
 * Handle D-Bus get_time_fmt method. Sets the time format locally and sends
 * time change indication with libosso. Saves config to
 * /home/user/.clockd.conf
 *
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_set_time_format_cb(DBusMessage *msg)
{
  DBusMessage *rsp;
  DBusError error = DBUS_ERROR_INIT;
  char *timeformat = NULL;
  dbus_bool_t success = FALSE;

  timeformat = 0;

  if (dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &timeformat,
                            DBUS_TYPE_INVALID))
  {

    DO_LOG(LOG_DEBUG, "Setting time format to %s",
           timeformat ? timeformat : "<null>");

    if (timeformat && *timeformat &&
        strlen(timeformat) < CLOCKD_GET_TIMEFMT_SIZE)
    {
      strcpy(time_format, timeformat);
      DO_LOG(LOG_DEBUG, "time format changed to '%s'", timeformat);

      if (!save_conf())
        success = TRUE;
    }
  }
  else
  {
    DO_LOG(LOG_ERR, "server_set_time_format_cb() %s : %s : %s",
           dbus_message_get_member(msg), error.name, error.message);
  }

  dbus_error_free(&error);
  rsp = server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);

  if (success)
    server_send_time_change_indication(0);

  return rsp;
}

/**
 * Handle D-Bus get_time_fmt method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_get_time_format_cb(DBusMessage *msg)
{
  const char *s = time_format;

  return server_new_rsp(msg, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus get_default_tz method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_get_default_tz_cb(DBusMessage *msg)
{
  const char *s = default_tz;

  return server_new_rsp(msg, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus get_tz method.
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_get_tz_cb(DBusMessage *msg)
{
  const char *s = server_tz;

  return server_new_rsp(msg, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus get_autosync method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_get_autosync_cb(DBusMessage *msg)
{
  dbus_bool_t as = !!autosync;

  return server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &as, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus have_opertime method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_have_opertime_cb(DBusMessage *msg)
{
  dbus_bool_t nt = net_time_setting;

  return server_new_rsp(msg, DBUS_TYPE_BOOLEAN, &nt, DBUS_TYPE_INVALID);
}

/**
 * Handle D-Bus get_time method
 * @param msg  request message
 * @return     NULL if fails, pointer to response DBusMessage if OK
 */
static DBusMessage *
server_get_time_cb(DBusMessage *msg)
{
  dbus_int32_t t = internal_get_time();

  return server_new_rsp(msg, DBUS_TYPE_INT32, &t, DBUS_TYPE_INVALID);
}

/**
 * Handle requests (methods or signals) coming via D-Bus
 *
 * @param conn       D-Bus connection
 * @param msg        D-Bus message
 * @param user_data  User data (not used)
 *
 * @return  DBusHandlerResult
 */
static DBusHandlerResult
server_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
  const char *iface = dbus_message_get_interface(msg);
  const char *member = dbus_message_get_member(msg);
  const char *path = dbus_message_get_path(msg);
  int i;

  if (!iface || !member || !path)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (!strcmp(iface, CSD_INTERFACE) && !strcmp(path, CSD_PATH))
  {
    if (!strcmp(member, CSD_NETWORK_TIMEINFO_CHANGE) &&
        dbus_message_is_signal(msg, CSD_INTERFACE, CSD_NETWORK_TIMEINFO_CHANGE))
    {
        handle_csd_net_time_change(msg);
    }
    else if (!strcmp(member, CSD_REGISTRATION_STATUS_CHANGE)
             && dbus_message_is_signal(msg, CSD_INTERFACE,
                                       CSD_REGISTRATION_STATUS_CHANGE))
    {
      DO_LOG(LOG_DEBUG, "CSD_REGISTRATION_STATUS_CHANGE received");
      mcc_tz_handle_registration_status_reply(msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  else if (!strcmp(iface, MCE_INTERFACE) && !strcmp(path, MCE_PATH) &&
           !strcmp(member, MCE_MODE_CHANGE) &&
           dbus_message_is_signal(msg, MCE_INTERFACE, MCE_MODE_CHANGE) &&
           net_time_changed_time)
  {
    DO_LOG(LOG_DEBUG, "got MCE normal/flight mode change indication");
    net_time_changed_time = 0;
  }
  else if (!strcmp(iface, "com.nokia.clockd") &&
           !strcmp(path, "/com/nokia/clockd"))
  {
    DBusMessage *reply = NULL;

    if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL)
    {
      for (i = 0; ; i++)
      {
        if (!server_callbacks[i].member)
        {
          DO_LOG(LOG_DEBUG, "server_filter() unknown member %s", member);
          reply =
              dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, member);
          break;
        }

        if (!strcmp(server_callbacks[i].member, member))
        {
          reply = server_callbacks[i].callback(msg);
          break;
        }
      }
    }

    if (!reply && !dbus_message_get_no_reply(msg))
      reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, member);

    if (reply)
    {
      dbus_connection_send(conn, reply, NULL);
      dbus_connection_flush(conn);
      dbus_message_unref(reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int
set_net_timezone(const char *tzname)
{
  char buf[256];
  struct stat stat_buf;

  memset(buf, 0, sizeof(buf));

  DO_LOG(LOG_DEBUG, "set_net_timezone: tz = %s", tzname ? tzname : "NULL");

  if (!tzname)
    goto err;

  snprintf(buf, sizeof(buf), "/usr/share/zoneinfo/%s", tzname + 1);
  if (stat(buf, &stat_buf))
    goto err;

  DO_LOG(LOG_DEBUG, "zone '%s' exists", buf);

  if (!internal_set_tz(tzname))
  {
    next_dst_change(time(0), 0);
    return 0;
  }

err:

  DO_LOG(LOG_WARNING, "zone '%s' not defined", buf);

  return -1;
}

/**
 * Set system and RTC time using suidroot rclockd helper
 * @param t  Current time
 * @return   0 if OK, -1 if error
 */
static int
server_set_time(time_t tick)
{
  int rv;

  rv = internal_set_time(tick);
  next_dst_change(tick, 0);

  return rv;
}

static void
server_set_operator_tz_cb(const char *tz)
{
  int st;

  if (tz)
  {
    DO_LOG(LOG_DEBUG, "server_set_operator_tz_cb(): tz = %s", tz);

    snprintf(saved_server_opertime_tz, sizeof(saved_server_opertime_tz), ":%s",
             tz);
    snprintf(server_tz, sizeof(server_tz) - 1, "/%s", &saved_server_opertime_tz[1]);
    st = internal_set_tz(server_tz);

    DO_LOG(LOG_DEBUG,
           "server_set_operator_tz_cb(): set_tz returned error code =  %d", st);

    internal_setenv_tz(server_tz);
    dump_date(server_tz);
    save_conf();
    next_dst_change(time(0), false);
    server_send_time_change_indication(0);
  }
  else
    DO_LOG(LOG_ERR, "server_set_operator_tz_cb(): tz = <null> !!!");
}

/**
 * Check DST change and send time change indication when DST changes.
 */
static gboolean
handle_alarm()
{
  DO_LOG(LOG_DEBUG, "handle_alarm: was_dst=%d, daylight=%d", was_dst,
         internal_get_dst(0));

  if (was_dst != internal_get_dst(0))
  {
    DO_LOG(LOG_INFO, "DST changed to %s", internal_get_dst(0) ? "on" : "off");
    server_send_time_change_indication(internal_get_time());
  }

  next_dst_change(time(0), false);

  return FALSE;
}

static void
next_dst_change(time_t tick, bool keep_alarm_timer)
{
  bool is_dst_now;
  time_t next;
  const int two_weeks = 14 * 24 * 60 * 60;

  if (!keep_alarm_timer)
    g_source_remove(alarm_timer_id);

  is_dst_now = internal_get_dst(tick);
  next = two_weeks;

  if (is_dst_now != internal_get_dst(tick + two_weeks))
  {
    time_t timeout = tick;
    int max_timeout = two_weeks;
    int i = 0;

    DO_LOG(LOG_DEBUG, "next_dst_change: dst change is in near future");

    do
    {
      max_timeout = (max_timeout + 1) / 2;

      if (is_dst_now == internal_get_dst(max_timeout + timeout))
        timeout += max_timeout;

      i++;
    }
    while (i != 21);

    next = timeout + max_timeout - tick;
  }

  DO_LOG(LOG_DEBUG, "next_dst_change: after %lu seconds (max timeout is %lu)\n",
         (unsigned long)next, (unsigned long)two_weeks);

  alarm_timer_id = g_timeout_add(1000 * next, handle_alarm,
                                 (gpointer)dbus_system_connection);
}

static int
set_network_time(bool save_config)
{
  struct tms buffer;
  clock_t now = times(&buffer);
  time_t t;
  int rv = -1;

  t = net_time_changed_time + (now - net_time_last_changed_ticks) / sysconf(2);

  if (!server_set_time(t))
  {
    if (saved_server_opertime_tz[0] &&
        internal_tz_cmp(&server_tz[1], &saved_server_opertime_tz[1]))
    {
      snprintf(server_tz, sizeof(server_tz), "/%s", &saved_server_opertime_tz[1]);
      set_net_timezone(server_tz);
      internal_setenv_tz(server_tz);
    }

    dump_date(server_tz);

    if (save_config)
    {
      save_conf();
      server_send_time_change_indication(t);
    }

    rv = 0;
  }

  return rv;
}

/**
 * Deinitialize D-Bus server
 */
void
server_quit(void)
{
  DO_LOG(LOG_DEBUG, "shutting down");

  if (dbus_connection)
  {
    mcc_tz_utils_quit();
    dbus_bus_remove_match(dbus_connection, MCE_MATCH_RULE, 0);
    dbus_bus_remove_match(dbus_connection, CSD_TIMEINFO_CHANGE_MATCH_RULE, 0);
    dbus_connection_unref(dbus_connection);
    dbus_connection = 0;
  }

  if (dbus_system_connection)
  {
    dbus_connection_remove_filter(dbus_system_connection, server_filter, 0);
    dbus_connection_unref(dbus_system_connection);
    dbus_system_connection = 0;
  }
}

static void
server_init_autosync()
{
  const char *s = getenv("CLOCKD_NET_TIME");

  if (s)
  {
    if (!strcmp(s, "disabled"))
    {
      net_time_setting = 0;
      autosync = 0;
      net_time_disabled_env = 1;
      DO_LOG(LOG_DEBUG,  "default network time setting is disabled");
    }
    else if (!strcmp(s, "yes"))
    {
      net_time_setting = 1;
      autosync = 1;
      DO_LOG(LOG_DEBUG,
             "default network time setting is enabled, autosync is on");
    }
    else if (!strcmp(s, "no"))
    {
      net_time_setting = 1;
      autosync = 0;
      DO_LOG(LOG_DEBUG,
             "default network time setting is enabled, autosync is off");
    }
    else
    {
      DO_LOG(LOG_ERR, "default invalid environment setting %s=\"%s\"",
             "CLOCKD_NET_TIME", s);
    }
  }
}

static void
server_init_time_format()
{
  const char *s = getenv("CLOCKD_TIME_FORMAT");

  if (s)
  {
    if (*s == '%')
      snprintf(time_format, sizeof(time_format), "%s", s);
    else
      snprintf(&time_format[1], sizeof(time_format) - 1, "%s", s);

    DO_LOG(LOG_DEBUG, "default time format set to \"%s\"", time_format);
  }
}

static void
server_init_default_tz()
{
  const char *s = getenv("CLOCKD_DEFAULT_TZ");

  if (s)
  {
    snprintf(default_tz, sizeof(default_tz), "%s", s);
    DO_LOG(LOG_DEBUG, "default timezone is \"%s\"", default_tz);
  }
}

/**
 * Read /home/user/.clockd.conf contents
 * @return  0 if OK, -1 if error
 */
static int
read_conf()
{
  struct stat st;
  FILE *fp;
  char line[512];

  /* FIXME - hardcoded home directory */
  if (stat("/home/user/", &st))
  {
    DO_LOG(LOG_ERR, "problems with directory %s (%s)", "/home/user/",
           strerror(errno));
  }

  fp = fopen("/home/user/.clockd.conf", "r");

  if (!fp)
  {
    DO_LOG(LOG_DEBUG, "failed to read file %s (%s)", "/home/user/.clockd.conf",
           strerror(errno));
    return -1;
  }

  line[sizeof(line) - 1] = 0;

  while (1)
  {
    char *p = NULL;

    do
    {
      if (!fgets(line, sizeof(line) - 1, fp))
      {
        DO_LOG(LOG_DEBUG, "configuration file %s read",
               "/home/user/.clockd.conf");
        fclose(fp);
        return 0;
      }

      if (line[0] == '#')
        continue;

      p = strrchr(line, '\n');
      if (p)
        *p = 0;

      p = strrchr(line, '\r');
      if (p)
        *p = 0;

      p = strchr(line, '=');
    }
    while (!p);

    *p = 0;
    p++;

    if (!strcmp(line, "time_format"))
    {
      snprintf(time_format, sizeof(time_format), "%s", p);
      DO_LOG(LOG_DEBUG, "read_conf: %s=%s", "time_format", time_format);
    }
    else if (!strcmp(line, "autosync"))
    {
      if (!net_time_disabled_env)
      {
        autosync = atoi(p) > 0;
        DO_LOG(LOG_DEBUG, "read_conf: %s=%d", "autosync", autosync);
      }
      else
        DO_LOG(LOG_DEBUG, "read_conf: autosync disabled by env");
    }
    else if (!strcmp(line, "net_tz"))
    {
      snprintf(server_tz, sizeof(server_tz), "%s", p);
      DO_LOG(LOG_DEBUG, "read_conf: %s=%s", "net_tz", server_tz);
    }
    else if (!strcmp(line, "restore_tz"))
    {
      snprintf(restore_tz, sizeof(restore_tz), "%s", p);
      DO_LOG(LOG_DEBUG, "read_conf: %s=%s", "restore_tz", restore_tz);
    }
  }

  return 0;
}

static int
get_autosync(void)
{
  return autosync;
}

/**
 * Init D-Bus server
 *
 * The following is set up:
 *
 * - settings from environment (/etc/clockd/clockd-settings.default)
 * - settings from configuration file (/home/user/.clockd.conf)
 * - D-Bus connections
 * - libosso init
 * - timezone from /etc/localtime or /home/user/.clockd.conf
 *
 * @return  0 if OK, -1 if error
 */
int
server_init()
{
  DBusError error = DBUS_ERROR_INIT;
  int retries;
  int rv = -1;

  DO_LOG(LOG_INFO, "starting up");

  server_init_autosync();
  server_init_time_format();
  server_init_default_tz();
  read_conf();

  if (restore_tz[0])
  {
    internal_set_tz(restore_tz);
    restore_tz[0] = 0;
    save_conf();
  }

  if (server_tz[0])
    internal_setenv_tz(server_tz);
  else
  {
    char buf[512];
    ssize_t bytes = readlink("/etc/localtime", buf, sizeof(buf) - 1);

    if (bytes > 0)
    {
      buf[bytes] = 0;

      if (strstr(buf, "/usr/share/zoneinfo/"))
      {
        snprintf(server_tz, sizeof(server_tz), ":%s",
                 &buf[strlen("/usr/share/zoneinfo/")]);
      }
      else
        snprintf(server_tz, sizeof(server_tz), ":%s", buf);
    }

    internal_setenv_tz(":/etc/localtime");
  }

  DO_LOG(LOG_DEBUG,
         "timezone set to '%s', operator time is %s, network time autosync is %s, time format is %s",
         server_tz,
         net_time_setting ? "enabled" : "disabled",
         autosync ? "enabled" : "disabled",
         time_format);

  was_dst = internal_get_dst(0);
  retries = 0;

  while (1)
  {
    dbus_system_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    if (dbus_system_connection)
      break;

    if (retries == 3)
    {
      DO_LOG(LOG_ERR, "dbus_bus_get(SYSTEM) %s", error.message);
      goto out;
    }

    DO_LOG(LOG_DEBUG, "dbus_bus_get(SYSTEM) %s - retry", error.message);

    retries++;
    dbus_error_init(&error);
    sleep(2);
  }

  if (dbus_bus_request_name(dbus_system_connection, "com.nokia.clockd", 4u, &error) != 1)
  {
    if (dbus_error_is_set(&error))
    {
      DO_LOG(LOG_ERR, "dbus_bus_request_name(%s) error %s", "com.nokia.clockd",
             error.message);
    }
    else
    {
      DO_LOG(LOG_ERR,
             "dbus_bus_request_name(%s), not primary owner of connection",
             "com.nokia.clockd");
    }

    sleep(2);
    goto out;
  }

  dbus_connection_setup_with_g_main(dbus_system_connection, 0);
  dbus_connection_set_exit_on_disconnect(dbus_system_connection, 0);

  if (!dbus_connection_add_filter(dbus_system_connection, server_filter, 0, 0))
  {
    DO_LOG(LOG_ERR, "server_init(%s), dbus_connection_add_filter failed",
           "com.nokia.clockd");
    goto out;
  }

  dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

  if (!dbus_connection)
  {
    DO_LOG(LOG_ERR, "dbus_bus_get(SYSTEM) %s", error.message);
    goto out;
  }

  dbus_connection_setup_with_g_main(dbus_connection, 0);
  dbus_connection_set_exit_on_disconnect(dbus_connection, 0);
  /* FIXME - error handling */
  dbus_bus_add_match(dbus_connection, CSD_TIMEINFO_CHANGE_MATCH_RULE, &error);
  dbus_bus_add_match(dbus_connection, MCE_MATCH_RULE, &error);
  dbus_connection_flush(dbus_connection);

  if (dbus_error_is_set(&error))
  {
    DO_LOG(LOG_ERR, "dbus_bus_add_match(%s) error %s",
           CSD_TIMEINFO_CHANGE_MATCH_RULE, error.message);
    goto out;
  }

  next_dst_change(time(0), 1);

  if (mcc_tz_utils_init(dbus_connection, get_autosync,
                        handle_csd_net_time_change,
                        server_set_operator_tz_cb))
  {
    DO_LOG(LOG_ERR, "mcc_tz_utils_init() error");
    goto out;
  }

  dump_date(server_tz);
  rv = 0;

out:
  dbus_error_free(&error);

  return rv;
}
