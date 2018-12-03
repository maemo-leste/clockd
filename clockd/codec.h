#ifndef CODEC_H
#define CODEC_H

/**
 * @brief Time services - D-Bus encoding and decoding.
 * @file  codec.h
 * @copyright GNU GPLv2 or later
 */

#include <time.h>
#include <dbus/dbus.h>

/**
 * Decode iteratively integer.
 *
 * @param iter  DBusMessageIter
 * @param pval  Pointer to int to store the decoded integer
 *
 * @return      -1 if error, 0 if success
 */
int decode_int (DBusMessageIter *iter, int *pval);

/**
 * Decode struct tm
 *
 * @param iter  DBusMessageIter
 * @param tm    Pointer to struct tm to store the decoded time
 *
 * @return      -1 if error, 0 if success
 */
int decode_tm (DBusMessageIter *iter, struct tm *tm);

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
int decode_ctm (DBusMessageIter *iter, struct tm *tm);

#endif  /* CODEC_H */
