# Copyright (C) 2025, 2026 Jason Self <j@jxself.org>
#
# This file is part of ZAbbrev.
#
# ZAbbrev is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# ZAbbrev is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with ZAbbrev. If not, see <https://www.gnu.org/licenses/>.

AC_PREREQ([2.69])
m4_define([ZABBREV_VERSION], m4_esyscmd_s([./version.sh]))
AC_INIT([zabbrev], [ZABBREV_VERSION], [j@jxself.org], [],
        [https://jxself.org/git/?p=zabbrev.git])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Check for C89 mode
AC_LANG([C])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h stdio.h errno.h getopt.h unistd.h \
                  dirent.h sys/stat.h sys/types.h time.h stdarg.h ctype.h])

# Checks for library functions.
AC_CHECK_FUNCS([getopt_long strcasecmp strdup strerror memchr opendir readdir])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])
AC_OUTPUT
