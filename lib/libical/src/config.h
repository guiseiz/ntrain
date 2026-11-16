#pragma once
// Portable single-threaded libical configuration for ESP32 newlib and host tests.
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <wctype.h>
#include <fcntl.h>
#define HAVE_GMTIME_R 1
#define HAVE_LOCALTIME_R 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_STRDUP 1
#define HAVE_SNPRINTF 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV 1
#define ICAL_ERRORS_ARE_FATAL 0
#define ICAL_ALLOW_EMPTY_PROPERTIES 0
#define PACKAGE_DATA_DIR "/no-libical-tzdata"
#define PACKAGE_VERSION "3.0.20"
#ifdef ESP32
#define SIZEOF_TIME_T 4
#define bswap_32 __builtin_bswap32
#define bswap_64 __builtin_bswap64
#else
#define SIZEOF_TIME_T 8
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#define _unused(x) (void)x
