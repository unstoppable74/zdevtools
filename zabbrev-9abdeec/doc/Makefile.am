# Copyright (C) 2025 Jason Self <j@jxself.org>
#
# This file is part of ZAbbrev.
#
# ZAbbrev is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ZAbbrev is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ZAbbrev. If not, see <https://www.gnu.org/licenses/>.

info_TEXINFOS = zabbrev.texi
man_MANS = zabbrev.1

AM_MAKEINFOFLAGS = -D VERSION=$(PACKAGE_VERSION)

EXTRA_DIST = $(info_TEXINFOS) $(man_MANS)

noinst_DATA = version.texi
version.texi:
	@echo "@set VERSION $(PACKAGE_VERSION)" > $@

CLEANFILES = version.texi
