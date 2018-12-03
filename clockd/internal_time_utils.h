#ifndef INTERNAL_TIME_UTILS_H
#define INTERNAL_TIME_UTILS_H

/**
 * @brief Time services - usefull wrappers of system time services.
 *
 * @file  internal_time_utils.h
 * Utilities above system time servises.
 *
 * @copyright GNU GPLv2 or later
 */


/**
 * Get current time. Returns system time.
 * @return  count of ticks from Epoch
 */
time_t internal_get_time (void);

/**
 * Set current time. Update system time.
 * @param t  count of ticks from Epoch to be set as current time
 * @return   0 if no error, error code otherwise
 */
int internal_set_time(time_t t);

/**
 * Get daylight state
 * @param tick  Current time, 0 if not known
 * @return      0 if no DST, 1 if DST
 */
int internal_get_dst(time_t tick);

/**
 * Get UTC offset
 *
 * @param tick  Current time, 0 if not known
 * @param dst   non-zero if daylight is counted in
 *
 * @return      Offset in secs
 */
int internal_get_utc_offset (time_t tick, int dst);

/**
 * Set system timezone
 * @param tz  Timezone
 * @return    0 if OK, !=0 if fails
 */
int internal_set_tz(const char *tz);

/**
 * Utility function used to set timezone temporarily. It is needed to
 * calculate time in timezone different from system one. It is used in pair
 * with internal_tz_res which restores previous timezone.
 *
 * @param old  Pointer to store current timezone name until internal_tz_res
 *             called. Note: the memory is allocated for this purpose. It
 *             will be freed automatically inside internal_tz_res.
 * @param tz   Temporal timezone
 */
void internal_tz_set(char **old, const char *tz);

/**
 * Utility function used to restore previous timezone changed by
 * internal_tz_set.
 *
 * @param old  Pointer to timezone name which was provided by
 *             internal_tz_set. Note: the memory allocated to store the name
 *             is freed after call of internal_tz_res. The pointer becomes 0
 *             at the end.
 */
void internal_tz_res(char **old);

/**
 * Make time_t from struct tm. Like mktime() but timezone can be given.
 *
 * @param tm  See mktime()
 * @param tz  Time zone variable. All formats that glibc supports can be
 *            given. NULL if current zone is used.<br>
 *            See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return    Time since Epoch or 0 if error
 */
time_t internal_mktime_in(struct tm *tm, const char *tz);

/**
 * Converts UTC time into local time for given timezone
 *
 * @param utc_tm  UTC time in tm structure
 * @param result  pointer to tm structure to place result
 * @param tz      Time zone variable. All formats that glibc supports can be
 *                given. NULL if current zone is used.<br>
 *                See http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @return        result pointer or NULL if error
 */
struct tm *internal_localtime_r_in(struct tm *utc_tm,
                                   struct tm *result,
                                   const char *tz);

/**
 * Set TZ environment variable and re-init time module of glibc to use given
 * timezone. If required it converts timezone from internal clockd format to
 * known by glibc.
 *
 * @param tzname  Time zone variable
 * @return        0 if OK or error code
 */
int internal_setenv_tz(const char *tzname);

/**
 * Helper function used to compare TZs
 *
 * @param firstTZName   First TZ Name
 * @param secondTZName  Second TZ Name
 *
 * @return  0 if TZ are equal, not 0 otherwise
 */
int internal_tz_cmp(const char *firstTZName, const char *secondTZName);

/**
 * Checks if timezone name is valid for glibc. See
 * http://www.gnu.org/software/libtool/manual/libc/TZ-Variable.html
 *
 * @param zone  Tested timezone name
 * @return      0 if valid or -1 if invalid
 */
int internal_check_timezone(const char *zone);

#endif  /* INTERNAL_TIME_UTILS_H */
