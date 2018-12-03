/**
 * @brief D-Bus encoding/decoding utilities.
 *
 * @file  codec.c
 *
 * Utilites for encoding/decoding dbus messages using message iterators.
 * Useful only because libdbus does not expose some internal functions via
 * the public API.
 *
 * @copyright GNU GPLv2 or later
 */

#include <string.h>
#include <stdio.h>

#include "codec.h"
#include "logging.h"

/**
 * Decode iteratively integer.
 *
 * @param iter  DBusMessageIter
 * @param pval  Pointer to int to store the decoded integer
 *
 * @return      -1 if error, 0 if success
 */
int
decode_int(DBusMessageIter *iter, int *pval)
{
  int type;
  int rv = -1;
  dbus_int32_t dbus_val = 0;

  type = dbus_message_iter_get_arg_type(iter);

  if (type == DBUS_TYPE_INT32)
  {
    dbus_message_iter_get_basic(iter, &dbus_val);
    dbus_message_iter_next(iter);
    rv = 0;
  }

  *pval = dbus_val;

  return rv;
}

/**
 * Decode struct tm
 *
 * @param iter  DBusMessageIter
 * @param tm    Pointer to struct tm to store the decoded time
 *
 * @return      -1 if error, 0 if success
 */
int
decode_tm(DBusMessageIter *iter, struct tm *tm)
{
  int rv = 0;

  if (decode_int(iter, &tm->tm_sec) ||
      decode_int(iter, &tm->tm_min) ||
      decode_int(iter, &tm->tm_hour) ||
      decode_int(iter, &tm->tm_mday) ||
      decode_int(iter, &tm->tm_mon) ||
      decode_int(iter, &tm->tm_year) ||
      decode_int(iter, &tm->tm_wday) ||
      decode_int(iter, &tm->tm_yday) ||
      decode_int(iter, &tm->tm_isdst))
  {
    rv = -1;
  }

  return rv;
}

/**
 * Decode NET_TIME_IND to struct tm
 *
 * The information may include the current date (day-month-year) and time
 * (hour-minute-second) in UTC (Coordinated Universal Time), which is in
 * practice same as GMT (Greenwich Mean Time).
 *
 * Daylight Saving Time
 *
 * - NET_DST_INFO_NOT_AVAIL 0x64
 * - NET_DST_1_HOUR 0x01
 * - NET_DST_2_HOURS 0x02
 * - NET_DST_0_HOUR 0x00
 *
 * Example 1: <br>
 * if the time is 12:00:00 and time zone is GMT+2 hours and daylight saving
 * time information is NET_DST_1_HOUR then the local time is 12:00:00 + 2
 * hours = 14:00:00. The Universal time is 12:00:00, the daylight saving is
 * in use, so the normal time in that area would be 13:00:00.
 *
 * Example 2: <br>
 * if universal time information is missing, but time zone information
 * indicates GMT - 10 hours, then the local time is 02:00:00 when universal
 * time is 12:00:00.
 *
 * Example 3: <br>
 * if universal time information is missing and time zone information is
 * missing, but daylight saving time information indicate NET_DST_1_HOUR then
 * it can be known that daylight saving of 1 hour is used currently in the
 * area and the current local time is +1 hours from the normal local time.
 * However, if time zone is not known it is not possible to determine the
 * local time. (The time zone might be known by some other means.)
 *
 * See:
 *
 * - http://www.timeanddate.com/worldclock/
 * - http://www.timeanddate.com/library/abbreviations/timezones/na/pst.html
 * - http://www.timeanddate.com/library/abbreviations/timezones/eu/eet.html
 *
 * - UTC time is 22:00 1st May 2008
 * - What is the YMDhms in Finland? 2. May 2008, 00:00:00
 *
 * - What is the value of Timezone in Finland? UTC+2 hours EET
 * - What is the YMDhms in California? San Francisco (U.S.A. - California)
 *   1. May 2008, 14:00:00
 *
 * - What is the value of Timezone in California ? UTC-8 hours PST
 *
 * @param iter  DBusMessageIter
 * @param tm    Pointer to struct tm to store the decoded time. tm->tm_yday
 *              is the UTC offset in secs.
 *
 * @return      -1 if error, 0 if success
 */
int
decode_ctm(DBusMessageIter *iter, struct tm *tm)
{
  bool invalid;
  int is_dst;
  int tz;

  memset(tm, 0, sizeof(*tm));

  if (decode_int(iter, &tm->tm_year) ||
      decode_int(iter, &tm->tm_mon) ||
      decode_int(iter, &tm->tm_mday) ||
      decode_int(iter, &tm->tm_hour) ||
      decode_int(iter, &tm->tm_min) ||
      decode_int(iter, &tm->tm_sec) ||
      decode_int(iter, &tm->tm_yday) ||
      decode_int(iter, &tm->tm_isdst))
  {
    return -1;
  }

  DO_LOG(LOG_DEBUG, "network time %d.%d.%d %d:%d:%d tz=%d isdst=%d",
         tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
         tm->tm_sec, tm->tm_yday, tm->tm_isdst);

  if (tm->tm_year == 100 || tm->tm_mon == 100 || tm->tm_mday == 100 ||
      tm->tm_hour == 100 || tm->tm_min == 100 || tm->tm_sec == 100)
  {
    invalid = 1;
  }

  if (invalid && tm->tm_yday == 100 && tm->tm_isdst == 100)
  {
    DO_LOG(LOG_DEBUG, "operator does not support network time");
    return -1;
  }

  if (invalid)
  {
    time_t tick = time(0);
    struct tm tp = {0, };

    gmtime_r(&tick, &tp);
    tm->tm_year = tp.tm_year;
    tm->tm_mon = tp.tm_mon;
    tm->tm_mday = tp.tm_mday;
    tm->tm_hour = tp.tm_hour;
    tm->tm_min = tp.tm_min;
    tm->tm_sec = tp.tm_sec;

    DO_LOG(LOG_DEBUG, "ignoring invalid time stamp, using current time");
  }
  else
  {
    tm->tm_year = tm->tm_year + 100;
    tm->tm_mon = tm->tm_mon - 1;
  }

  if (tm->tm_isdst == 100 || tm->tm_isdst < 0 || tm->tm_isdst > 2)
    is_dst = 100;
  else
    is_dst = tm->tm_isdst;

  if (tm->tm_yday == 100)
    tz = 100;
  else
  {
    tz = tm->tm_yday & 0x3F;

    if (tm->tm_yday & 0x80)
      tz = -tz;
  }

  tm->tm_isdst = is_dst;
  tm->tm_yday = tz;

  DO_LOG(LOG_DEBUG, "network time fixed %d.%d.%d %d:%d:%d tz=%d isdst=%d",
         tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
         tm->tm_sec, tm->tm_yday, tm->tm_isdst);

  return 0;
}
