/**
 * @brief clockd helper (suidroot) program to set system time
 * @file  rclockd.c
 * @copyright GNU GPLv2 or later
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <linux/rtc.h>

#include "logging.h"

bool clockd_debug_mode = false;

#define SET_UID(uid) set_uid(uid, __FUNCTION__)

static __uid_t
set_uid(__uid_t uid, const char *f)
{
  if (uid && setuid(0))
  {
    DO_LOG(LOG_ERR, "%s(), setuid(for %u) failed %s", f, uid, strerror(errno));
    return 0;
  }

  return uid;
}

/**
 * See man 8 hwclock
 */
static int
set_time(const char *s)
{
  unsigned long sec = strtoul(s, 0, 10);
  __uid_t uid = SET_UID(getuid());
  struct timeval tv;
  int rv = -1;

  tv.tv_sec = sec;
  tv.tv_usec = 0;

  if (settimeofday(&tv, NULL))
  {
    /* See man 4 rtc */
    time_t timer = sec;
    int fd = open("/dev/rtc", O_RDWR);

    if (fd != -1)
    {
      struct tm *tm = gmtime(&timer);

      if (tm)
      {
        if (ioctl(fd, RTC_SET_TIME, tm) >= 0)
        {
          rv = 0;
          DO_LOG(LOG_DEBUG, "time set successfully to %lu", sec);
        }
        else
          DO_LOG(LOG_ERR, "ioctl(RTC_SET_TIME) error %s", strerror(errno));
      }
      else
        DO_LOG(LOG_ERR, "gmtime() failed");

      close(fd);
    }
    else
      DO_LOG(LOG_ERR, "open(%s) error %s", "/dev/rtc", strerror(errno));
  }
  else
    DO_LOG(LOG_ERR, "settimeofday() failed (%s)", strerror(errno));

  SET_UID(uid);

  return rv;
}

static int
set_tz(const char *s)
{
  char path[256];
  struct stat stat_buf;
  __uid_t uid = SET_UID(getuid());
  int rv = -1;

  if (s[1] == '/')
    snprintf(path, sizeof(path), "%s", s + 1);
  else
    snprintf(path, sizeof(path), "/usr/share/zoneinfo/%s", s + 1);

  DO_LOG(LOG_DEBUG, "set_tz(), path=%s", path);

  if (stat(path, &stat_buf))
  {
    rename("/etc/localtime", "/etc/localtime.save");

    if (!symlink(path, "/etc/localtime"))
    {
      rv = 0;
      DO_LOG(LOG_DEBUG, "timezone changed to '%s'", path);
    }
    else
    {
      int st = rename("/etc/localtime.save", "/etc/localtime");

      DO_LOG(LOG_ERR, "set_tz()/symlink (%s), path=%s, %s",
             st ? "not recovered" : "recovered", path, strerror(errno));
    }
  }
  else
    DO_LOG(LOG_ERR, "set_tz()/stat, path=%s, %s", path, strerror(errno));

  SET_UID(uid);

  return rv;
}

int main(int argc, char **argv)
{
  int rv;

  if (argc != 3 || strcmp(argv[1], "clockd"))
  {
    fprintf(stderr, "%s is for clockd usage only\n", argv[0]);
    exit(2);
  }

  if (isdigit(argv[2][0]))
    rv = set_time(argv[2]);
  else
    rv = set_tz(argv[2]);

  if (!rv)
    exit(0);

  exit(1);
}
