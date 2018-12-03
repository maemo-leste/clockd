#ifndef CLOCK_DBUS_H
#define CLOCK_DBUS_H

/**
 * @brief Time services - D-Bus definitions.
 *
 * @file  clock_dbus.h
 * Defines D-Bus service and method names to communicate with clockd.
 *
 * @copyright GNU GPLv2 or later
 */


/**
 * CLOCKD_SET_TIME:
 *
 * - I: int32 time
 * - O: bool success
 *
 * Method to set time.
 */
#define CLOCKD_SET_TIME "set_time"

/**
 * CLOCKD_GET_TIME:
 *
 * - I: n/a
 * - O: int32 time
 *
 * Method to set time.
 */
#define CLOCKD_GET_TIME "get_time"

/**
 * CLOCKD_GET_TZ:
 *
 * - I: n/a
 * - O: string tz
 *
 * Method to get timezone.
 */
#define CLOCKD_GET_TZ "get_tz"

/**
 * CLOCKD_SET_TZ:
 *
 * - I: string tz
 * - O: bool success
 *
 * Method to set timezone.
 */
#define CLOCKD_SET_TZ "set_tz"

/**
 * CLOCKD_GET_TIMEFMT:
 *
 * - I: n/a
 * - O: string fmt
 *
 * Method to get time formatter.
 */
#define CLOCKD_GET_TIMEFMT "get_timefmt"

/**
 * CLOCKD_GET_DEFAULT_TZ:
 *
 * - I: n/a
 * - O: string fmt
 *
 * Method to get time formatter.
 */
#define CLOCKD_GET_DEFAULT_TZ "get_default_tz"

/**
 * CLOCKD_SET_TIMEFMT:
 *
 * - I: string fmt
 * - O: bool success
 *
 * Method to set time formatter.
 */
#define CLOCKD_SET_TIMEFMT "set_timefmt"

/**
 * CLOCKD_SET_AUTOSYNC:
 *
 * - I: boolean enabled
 * - O: boolean success
 *
 * Method to set automatic (cell) network time sync mode
 */
#define CLOCKD_SET_AUTOSYNC "set_autosync"

/**
 * CLOCKD_GET_AUTOSYNC:
 *
 * - I: n/a
 * - O: boolean enabled
 *
 * Method to get automatic (cell) network time sync mode
 */
#define CLOCKD_GET_AUTOSYNC "get_autosync"

/**
 * CLOCKD_HAVE_OPERTIME:
 *
 * - I: n/a
 * - O: boolean accessible
 *
 * Method to ask whether operator network time is configured (hardware)
 */
#define CLOCKD_HAVE_OPERTIME "have_opertime"

/**
 * CLOCKD_ACTIVATE_NET_TIME:
 *
 * - I: n/a
 * - O: boolean success
 *
 * Method to activate network time change.
 */
#define CLOCKD_ACTIVATE_NET_TIME "activate_net_time"

/**
 * CLOCKD_NET_TIME_CHANGED:
 *
 * - I: n/a
 * - O: int32 time (0 if no change)
 * - O: string tz
 *
 * Method to get info whether network time has been changed.
 */
#define CLOCKD_NET_TIME_CHANGED "net_time_changed"

/**
 * CSD_SERVICE:
 * The name of the csd service.
 */
#define CSD_SERVICE "com.nokia.phone.net"

/**
 * CSD_PATH:
 * The object path for the csd daemon.
 */
#define CSD_PATH "/com/nokia/phone/net"

/**
 * CSD_INTERFACE:
 * The interface the commands use.
 */
#define CSD_INTERFACE "Phone.Net"

/**
 * CSD_NETWORK_TIMEINFO_CHANGE:
 * Signal from CSD when automatic time has been changed.
 */
#define CSD_NETWORK_TIMEINFO_CHANGE "network_time_info_change"

/**
 * CSD_REGISTRATION_STATUS_CHANGE:
 * Signal from CSD when registration status has been changed.
 */
#define CSD_REGISTRATION_STATUS_CHANGE "registration_status_change"

/**
 * CSD_GET_REGISTRATION_STATUS:
 * Method from CSD to get current registration status.
 */
#define CSD_GET_REGISTRATION_STATUS "get_registration_status"

/**
 * CSD_GET_NETWORK_TIMEINFO:
 * Method from CSD to get current network time.
 */
#define CSD_GET_NETWORK_TIMEINFO "get_network_time_info"

/**
 * CSD_TIMEINFO_CHANGE_MATCH_RULE:
 * Rule to catch the network time signal.
 */
#define CSD_TIMEINFO_CHANGE_MATCH_RULE \
  "type='signal',interface='Phone.Net',member='network_time_info_change'"

/**
 * CSD_REGISTRATION_CHANGE_MATCH_RULE:
 * Rule to catch the registration status signal.
 */
#define CSD_REGISTRATION_CHANGE_MATCH_RULE \
  "type='signal',interface='Phone.Net',member='registration_status_change'"

/**
 * MCE_SERVICE:
 * The name of the MCE service.
 */
#define MCE_SERVICE "com.nokia.mce"

/**
 * MCE_PATH:
 * The object path for the csd daemon.
 */
#define MCE_PATH "/com/nokia/mce/signal"

/**
 * MCE_INTERFACE:
 * The interface the commands use.
 */
#define MCE_INTERFACE "com.nokia.mce.signal"

/**
 * MCE_MODE_CHANGE:
 * Signal from MCE when normal/flight mode has been changed.
 */
#define MCE_MODE_CHANGE "sig_device_mode_ind"

/**
 * MCE_MATCH_RULE:
 * Rule to catch the network time signal.
 */
#define MCE_MATCH_RULE \
  "type='signal',interface='com.nokia.mce.signal',member='sig_device_mode_ind'"

/**
 * CLOCKD_GET_TIMEFMT_SIZE:
 * Max time formatter size.
 */
#define CLOCKD_GET_TIMEFMT_SIZE 32

/**
 * CLOCKD_TZ_SIZE:
 * Max timezone size.
 */
#define CLOCKD_TZ_SIZE 256

#ifndef DBUS_TIMEOUT_USE_DEFAULT
#define DBUS_TIMEOUT_USE_DEFAULT -1
#endif

#endif  /* CLOCK_DBUS_H */
