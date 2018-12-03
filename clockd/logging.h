#ifndef LOGGING_H
#define LOGGING_H

/**
 * @brief clockd logging interface
 *
 * @file  logging.h
 * Interface to syslog and console debug printing
 *
 * @copyright GNU GPLv2 or later
 */

#include <syslog.h>
#include <stdbool.h>

extern bool clockd_debug_mode;

/** Macro to do all logging
 * @param __level  Message level, one of the following:
 *                 - \c LOG_CRIT - critical failure
 *                 - \c LOG_ERR - error condition, possibly resulting in
 *                   reduced functionality
 *                 - \c LOG_WARNING - abnormal condition, possibly resulting
 *                   in reduced functionality
 *                 - \c LOG_NOTICE - normal, but significant, condition
 *                 - \c LOG_INFO - informational message
 *                 - \c LOG_DEBUG - debugging message (NO-OP if not in debug
 *                   mode)
 * @param ...      Typically printf-style formatter and parameters
 */
#define DO_LOG(__level, ...) \
do {                                       \
  if (__level == LOG_DEBUG && !clockd_debug_mode) ;             \
  else {                                                        \
    if (clockd_debug_mode) {                                    \
      printf(MESTR __VA_ARGS__); putchar('\n'); fflush(stdout); \
    }                                                           \
    syslog(__level, __VA_ARGS__);                               \
  }                                                             \
} while(0)

/** Macro to do logging of slist of strings
 * @param __level  Message level, one of the following:
 *                 - \c LOG_CRIT - critical failure
 *                 - \c LOG_ERR - error condition, possibly resulting in
 *                   reduced functionality
 *                 - \c LOG_WARNING - abnormal condition, possibly resulting
 *                   in reduced functionality
 *                 - \c LOG_NOTICE - normal, but significant, condition
 *                 - \c LOG_INFO - informational message
 *                 - \c LOG_DEBUG - debugging message (NO-OP if not in debug
 *                   mode)
 * @param __slist  pointer to slist head
 */
#define DO_LOG_STR_SLIST(__level, __slist) \
{ \
  if(__level == LOG_DEBUG && !clockd_debug_mode) ; \
  else { \
    if(!__slist) \
    { \
      DO_LOG(__level, "GSList empty"); \
    } \
    else \
    { \
      GSList *iter = __slist; \
      DO_LOG(__level, "GSList count = %d", g_slist_length (__slist)); \
      while(iter) \
      { \
        DO_LOG(__level, "%s", (char*)iter->data); \
        iter = g_slist_next(iter); \
      } \
    } \
  } \
}

/**
 * Dump current date settings to syslog.
 * @param server_tz  current server timezone to be printed in log
 */
void dump_date(const char *server_tz);

/**
 * Log tm structure
 * @param tag  string describing tm
 * @param tm   tm to be logged
 */
void log_tm(const char *tag, const struct tm *tm);

#endif  /* LOGGING_H */
