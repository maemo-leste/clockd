#ifndef LIBTIME_H
#define LIBTIME_H

/**
 * @brief Interface to time functionality (libtime and clockd)
 *
 * @file  libtime.h
 *
 * This is the interface to time functions. <br>
 * Include also file <time.h>
 *
 * Link your application with -lrt
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CLOCKD_SERVICE:
 * The name of the clockd D-Bus service.
 */
#define CLOCKD_SERVICE "com.nokia.clockd"

/**
 * CLOCKD_PATH:
 * The D-Bus object path for the clockd daemon.
 */
#define CLOCKD_PATH "/com/nokia/clockd"

/**
 * CLOCKD_INTERFACE:
 * The D-Bus interface the commands/signal use.
 */
#define CLOCKD_INTERFACE "com.nokia.clockd"

/**
 * D-Bus signal sent when time settings has been changed.
 * Argument is int32 time. 0 means that time itself
 * has not been changed (i.e timezone, for example, has changed).
 */
#define CLOCKD_TIME_CHANGED "time_changed"

/**
 * Function to call when "time changed" indication (see osso_time_set_notification_cb)
 * has been received by the application.<br>
 * If the application does not wish to link with libosso, then it can listen to the
 * CLOCKD_TIME_CHANGED D-Bus signal, that clockd sends also.
 *
 * The time changed indication is broadcasted to all libtime users when:
 * - time is changed (time_set_time or by automatic network time change)
 * - time zone is changed (time_set_timezone or by automatic network time change)
 * - time formatter is changed (time_set_time_format)
 * - automatic network time setting is changed (time_set_autosync)
 *
 * @return     0 if OK, -1 if fails
 */
int time_get_synced(void);

/** Get current time - see time() */
time_t time_get_time(void);

/**
 * Set current system and RTC time
 * @param tick  Time since Epoch
 * @return      0 if OK, -1 if fails
 */
int time_set_time(time_t tick);

/**
 * Get the network time (in case that autosync is not
 * enabled and a network time change indication has received)
 *
 * @param tick  Supplied buffer to store network time (ticks since Epoch)
 * @param s     Supplied buffer to store network timezone
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'. -1 if network time has not been changed.
 *              Does not write more than size bytes (including the trailing "\0").
 *              If the output was truncated due to this limit then the return value
 *              is the  number  of  characters (not including the  trailing  "\0")
 *              which  would have been written to the final string if enough
 *              space had been available.
 *              Thus, a return value of size or more means that the output was truncated.
 */
int  time_get_net_time(time_t *tick, char *s, size_t max);

/**
 * Deprecated, see time_get_net_time
 */
int  time_is_net_time_changed(time_t *tick, char *s, size_t max);

/**
 * Set current system and RTC time according to operator network time in case
 * time_is_net_time_changed() indicates change
 *
 * @return     0 if OK, -1 if fails (for example if the network time has not changed)
 */
int time_activate_net_time(void);

/**
 * Make time_t from struct tm. Like mktime() but timezone can be given.
 *
 * @param tm  See mktime
 * @param tz  Time zone variable. All formats that glibc supports can be given.
 *            NULL if current zone is used.<br>
 *            See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return    Time since Epoch (0) if error
 */
time_t time_mktime(struct tm *tm, const char *tz);

/**
 * Get current time zone. May return empty string if time zone has not been set.<br>
 * Example output:<br>
 *   :US/Central                      <br>
 *   GMT+5:00GMT+4:00,0,365           <br>
 * The latter format is returned when network has indicated time zone change
 * (the actual location is not known)
 *
 * @param s     Supplied buffer to store timezone
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'.
 *              Does not write more than size bytes (including the trailing "\0").
 *              If the output was truncated due to this limit then the return value
 *              is the  number  of  characters (not including the  trailing  "\0")
 *              which  would have been written to the final string if enough
 *              space had been available.
 *              Thus, a return value of size or more means that the output was truncated.
 *              If an output error is encountered, a negative value is returned.
 */
int time_get_timezone(char *s, size_t max);

/**
 * Get current time zone name (like "EET")
 *
 * @param s     Supplied buffer to store tzname
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'.
 *              Does not write more than size bytes (including the trailing "\0").
 *              If the output was truncated due to this limit then the return value
 *              is the  number  of  characters (not including the  trailing  "\0")
 *              which  would have been written to the final string if enough
 *              space had been available.
 *              Thus, a return value of size or more means that the output was truncated.
 *              If an output error is encountered, a negative value is returned.
 */
int time_get_tzname(char *s, size_t max);

/**
 * Set current time zone ("TZ")
 *
 * @param tz  Time zone variable. All formats that glibc supports can be given.<br>
 *            See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html<br>
 *            It is, however recommended to use /usr/share/zoneinfo -method, for example:<br>
 *             :Europe/Rome<br>
 *            and not like this:<br>
 *              EST+5EDT,M4.1.0/2,M10.5.0/2
 *
 * @return    0 if OK, -1 if fails
 */
int time_set_timezone(const char *tz);

/**
 * Get current time in UTC. Inspired by time() and gmtime_r()
 * @param tm  Supplied buffer to store result
 * @return    0 if OK, -1 if error
 */
int time_get_utc(struct tm *tm);

/**
 * Get time in UTC.
 *
 * @param tick  Time
 * @param tm    Supplied buffer to store result
 *
 * @return    0 if OK, -1 if error
 */
int time_get_utc_ex(time_t tick, struct tm *tm);

/**
 * Get current local time. Inspired by time() and localtime_r().
 * @param tm  Supplied buffer to store tm
 * @return    0 if OK, -1 if error
 */
int time_get_local(struct tm *tm);

/**
 * Get local time.
 *
 * @param tick  Time
 * @param tm    Supplied buffer to store tm
 *
 * @return    0 if OK, -1 if error
 */
int time_get_local_ex(time_t tick, struct tm *tm);

/**
 * Get local time in given zone. Inspired by setting TZ temporarily and localtime_r().
 *
 * @param tick  Time since Epoch
 * @param tz    Time zone variable. All formats that glibc supports can be given.<br>
 *              See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 * @param tm    Supplied buffer to store tm
 *
 * @return    0 if OK, -1 if error
 */
int time_get_remote(time_t tick, const char *tz, struct tm *tm);

/**
 * Get the default time zone. Empty string is returned if no default zone has been defined.
 *
 * @param s     Supplied buffer to store the zone
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'.
 *              Does not write more than size bytes (including the trailing "\0").
 *              If the output was truncated due to this limit then the return value
 *              is the  number  of  characters (not including the  trailing "\0")
 *              which  would have been written to the final string if enough
 *              space had been available.
 *              Thus, a return value of size or more means that the output was truncated.
 *              If an output error is encountered, a negative value is returned.
 */
int time_get_default_timezone(char *s, size_t max);

/**
 * Get current time string formatter. Inspired by strftime().
 *
 * @param s     Supplied buffer to store formatter
 * @param max   Size of 's', including terminating NUL
 *
 * @return      Number of characters wrote to 's'.
 *              Does not write more than size bytes (including the trailing "\0").
 *              If the output was truncated due to this limit then the return value
 *              is the  number  of  characters (not including the  trailing "\0")
 *              which  would have been written to the final string if enough
 *              space had been available.
 *              Thus, a return value of size or more means that the output was truncated.
 *              If an output error is encountered, a negative value is returned.
 */
int time_get_time_format(char *s, size_t max);

/**
 * Set current time string formatter. Inspired by strftime().
 * @param fmt Formatter string
 * @return    0 if OK, -1 if fails
 */
int time_set_time_format(const char *fmt);

/**
 * Format given time to string. Inspired by strftime() and localtime_r().
 *
 * @param tm    Time
 * @param fmt   Formatter, see strftime and time_set_time_format and
 *              time_get_time_format, NULL if active formatter is used.
 * @param s     Supplied buffer to store result
 * @param max   Size of 's', including terminating NUL
 *
 * @return      See strftime()
 */
int time_format_time(const struct tm *tm, const char *fmt, char *s, size_t max);

/**
 * Get utc offset (secs west of GMT) in the named TZ. The current daylight
 * saving time offset is included.
 *
 * @param tz    Time zone, all formats that glibc supports can be given.
 *              NULL to use current tz. <br>
 *              See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return      Secs west of GMT. or -1 in case of error
 */
int time_get_utc_offset(const char *tz);

/**
 * Return if daylight-saving-time is in use in given time.
 *
 * @param tick  Time since Epoch
 * @param tz    Time zone, all formats that glibc supports can be given.
 *              NULL to use current tz. <br>
 *              See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return      Nonzero if daylight savings time is in effect, zero if not,
 *              -1 if error
 */
int time_get_dst_usage(time_t tick, const char *tz);

/** See difftime() */
double time_diff(time_t t1, time_t t2);

/**
 * Get time difference between two timezones
 *
 * @param tick          Time since Epoch
 * @param tz1           Timezone 1
 * @param tz2           Timezone 2
 *
 * @return              Local time in tz1 - local time in tz2
 */
int time_get_time_diff(time_t tick, const char *tz1, const char *tz2);

/**
 * Enable or disable automatic time settings based on cellular network time.
 * @param enable        Nonzero to enable, zero to disable
 * @return              0 if OK, -1 if error
 */
int  time_set_autosync(int  enable);

/**
 * Get the state of automatic time settings based on cellular network time.
 * @return  Non-zero if enabled, zero if disabled, -1 if error
 */
int time_get_autosync(void);

/**
 * Get info if the device supports network time updates (i.e. has CellMo).
 * @return  Nonzero if accessible, 0 if not, -1 if error
 */
int time_is_operator_time_accessible(void);

#ifdef __cplusplus
};
#endif

#endif  /* LIBTIME_H */
