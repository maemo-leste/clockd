#ifndef MCC_TZ_UTILS_H
#define MCC_TZ_UTILS_H

/**
 * @brief Time services - utils to work with country timezones.
 * @file  mcc_tz_utils.h
 * @copyright GNU GPLv2 or later
 */

typedef int (*GetAutosyncEnabled)();
typedef int (*HandleCsdNetTimeChange)(DBusMessage*);
typedef void (*SetOperatorTz)(const char*);

/**
 * Checks if given timezone name is in timezone list for current country
 *
 * @param tz_name  timezone name for searching
 *
 * @return         1 if given timezone name found in country_tz_name_list or
 *                 0 otherwise
 */
int mcc_tz_is_tz_name_in_country_tz_list(const char *tz_name);

/**
 * Guesses named timezone for current country which has the same DST and GMT
 * offset as requested in given UTC time. Returns first found name in tz_name
 * or NULL if not found.
 *
 * @param utc_tm   UTC time to search in
 * @param dst      DST for searching timezone in given UTC time
 * @param gmtoff   GMT offset for searching timezone in given UTC time
 * @param tz_name  name of found timezone for requested parameters or NULL if
 *                 not found It shall not be freed since it is pointer to
 *                 string contained in country_tz_name_list Also the user
 *                 must dup this string if it is needed in next mainloop
 *                 cycle since the list might be updated in-between.
 */
void mcc_tz_guess_tz_for_country_by_dst_and_offset(struct tm *tm,
                                                   int dst,
                                                   int gmtoff,
                                                   char **tz_name);

/**
 * If autoupdate is on, it sends get registration status request to CSD. It
 * launches procedure to update time if device is registered in network.
 *
 * Also the method asks D-Bus to notify about registration status change
 * signal if time autoupdate is on and remove this tracking if time
 * autoupdate is off. The signal is handled by server_filter (refer to
 * mcc_tz_init also).
 *
 * If autoupdate is on, it sends get registration status request to CSD. It
 * launches procedure to update time if device is registered in network.
 * Reply is catched by mcc_tz_registration_status_reply_dbus_cb callback.
 *
 * Also the method asks D-Bus to notify about registration status change
 * signal if time autoupdate is on and remove this tracking if time
 * autoupdate is off. The signal is handled by server_filter (refer to
 * mcc_tz_init also).
 */
void mcc_tz_setup_timezone_from_mcc_if_required(void);

/**
 * Handle reply on get registration status request to CSD. If registered in
 * home or roaming network and received MCC is not equal to cached one,
 * launch network time update attempt, since country (and timezone probably)
 * is changed.
 *
 * @param msg  D-Bus reply on get registration status request to CSD
 */
void mcc_tz_handle_registration_status_reply(struct DBusMessage *msg);

/**
 * Fuction to init mcc_tz module
 *
 * @param server_system_bus                  pointer to system DBUS
 * @param server_get_autosync_enabled        pointer to function to get
 *                                           autosync_enabled value
 * @param server_handle_csd_net_time_change  pointer to function to handle
 *                                           time info message from csd
 * @param server_set_operator_tz             pointer to function to set
 *                                           operator timezone
 *
 * @return  -1 if error, 0 otherwise
 */
int mcc_tz_utils_init(DBusConnection *server_system_bus,
                      GetAutosyncEnabled server_get_autosync_enabled,
                      HandleCsdNetTimeChange server_handle_csd_net_time_change,
                      SetOperatorTz server_set_operator_tz);

/**
 * Fuction to free resources allocated by mcc_tz module
 */
void mcc_tz_utils_quit(void);

#endif  /* MCC_TZ_UTILS_H */
