#
# Generic options
#
AM_CFLAGS = -Wall -Wmissing-prototypes -std=c99 -Os -g -fPIC
AM_CPPFLAGS = -I$(top_srcdir)/modules -D_GNU_SOURCE
AM_LDFLAGS = -g -Wl,--as-needed

ACLOCAL_AMFLAGS = -I m4

#
# Additional dirs
#
clockdincludedir = @includedir@/clockd

#
# Build targets
#
bin_PROGRAMS = clockd rclockd
lib_LTLIBRARIES = libtime.la
lib_LIBRARIES = libtime.a

libtime_a_SOURCES = libtime.c codec.c
libtime_a_CFLAGS = $(DBUS_CFLAGS) -DMESTR="\"$(PACKAGE_NAME):\""

clockd_SOURCES = sighnd.c clockd.c mainloop.c internal_time_utils.c mcc_tz_utils.c logging.c server.c
clockd_CFLAGS = $(DBUS_CFLAGS) $(GLIB_CFLAGS) $(CITYINFO_CFLAGS) $(DBUSGLIB_CFLAGS) $(GCONF_CFLAGS) -DMESTR="\"$(PACKAGE_NAME):\""
clockd_LDADD = $(DBUS_LIBS) $(GLIB_LIBS) $(CITYINFO_LIBS) $(DBUSGLIB_LIBS) $(GCONF_LIBS) libtime.a

rclockd_SOURCES = rclockd.c
rclockd_CFLAGS = -DMESTR="\"$(PACKAGE_NAME):\""

libtime_la_SOURCES = libtime.c
libtime_la_CFLAGS = $(DBUS_CFLAGS)
libtime_la_LIBADD = $(DBUS_LIBS)
libtime_la_LDFLAGS = $(AM_LDFLAGS) -pthread --shared

clockdinclude_HEADERS = libtime.h

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = libtime.pc

clockdconfossorfsscriptsdir=$(sysconfdir)/osso-rfs-scripts
clockdconfossorfsscripts_DATA = ../etc/osso-rfs-scripts/clockd-rfs.sh
clockdconfdbusdir=$(sysconfdir)/dbus-1/system.d
clockdconfdbus_DATA = ../etc/dbus-1/system.d/clockd.conf
clockdconfbackuprestoredir=$(sysconfdir)/osso-backup/restore.d/always
clockdconfbackuprestore_DATA = ../etc/osso-backup/restore.d/always/clockd_restart.sh
clockdconfbackupapplicationsdir = $(sysconfdir)/osso-backup/applications
clockdconfbackupapplications_DATA = ../etc/osso-backup/applications/clockd.conf

MAINTAINERCLEANFILES = Makefile.in
