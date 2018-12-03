/**
 * @brief clockd logging stuff (see logging.h)
 * @file  logging.c
 * @copyright GNU GPLv2 or later
 */

#include <time.h>
#include <stdio.h>
#include <string.h>

#include "logging.h"
#include "internal_time_utils.h"

bool clockd_debug_mode = false;

/**
 * Log tm structure
 * @param tag  string describing tm
 * @param tm   tm to be logged
 */
void
log_tm(const char *tag, const struct tm *tm)
{
  DO_LOG(LOG_DEBUG, "%s %04d-%02d-%02d %02d:%02d:%02d wd=%d yd=%d dst=%d off=(%g h)=(%ld secs) tz=%s",
         tag, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
         tm->tm_min, tm->tm_sec, tm->tm_wday, tm->tm_yday, tm->tm_isdst,
         (double)((long double)(signed int)tm->tm_gmtoff / (double)3600.0),
         tm->tm_gmtoff, tm->tm_zone);
}

/**
 * Dump current date settings to syslog.
 * @param server_tz  current server timezone to be printed in log
 */
void
dump_date(const char *server_tz)
{
  struct tm tm;
  time_t timer;

  memset(&tm, 0, sizeof(tm));
  timer = internal_get_time();
  localtime_r(&timer, &tm);

  DO_LOG(LOG_INFO,
         "Date now is %04d-%02d-%02d %02d:%02d:%02d (DST %s), TZ=%s, offset %d/%d, timezone=%ld, tzname=%s/%s",
         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
         tm.tm_sec, internal_get_dst(timer) ? "ON" : "OFF", server_tz,
         internal_get_utc_offset(timer, 1), internal_get_utc_offset(timer, 0),
         timezone, tzname[0], tzname[1]);
}
