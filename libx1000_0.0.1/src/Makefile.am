AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libx1000.la
libx1000_la_SOURCES = x1000_ldpreload.c x1000_memregions.c x1000_config.c
libx1000_la_CFLAGS = $(AM_CFLAGS)

bin_PROGRAMS = tc_dumpmemregions tc_dumpconfig
tc_dumpmemregions_SOURCES = ../test/dumpmemregions.c x1000_memregions.c
tc_dumpconfig_SOURCES = ../test/dumpconfig.c x1000_config.c

libx1000dir = $(sysconfdir)/libx1000.so.d
libx1000_DATA = ../test/defaults.conf ../test/tc_dumpconfig.conf
