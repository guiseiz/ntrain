"""Vendor a configured upstream libical 3.0.20 C core; no runtime network dependency.
Usage: python tools/vendor_libical.py SOURCE CMAKE_BUILD_DIRECTORY
Upstream commit: 57ce34025066935f356fd539e6deb7a7a9952618.
"""
import sys
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source, generated = map(Path, sys.argv[1:])
dest = root / 'lib/libical/src'
dest.mkdir(parents=True, exist_ok=True)
for folder in [source/'src/libical', generated/'src/libical']:
    for path in folder.iterdir():
        if path.suffix in ('.c', '.h'):
            shutil.copyfile(path, dest/path.name)
for name in ['LICENSE', 'COPYING', 'ReadMe.md']:
    if (source/name).exists(): shutil.copyfile(source/name, dest.parent/name)
config = '''#pragma once
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
'''
(dest/'config.h').write_text(config)
(dest.parent/'library.json').write_text('{"name":"libical","version":"3.0.20","build":{"flags":["-DHAVE_CONFIG_H","-DLIBICAL_ICAL_STATIC_DEFINE"]}}\n')
