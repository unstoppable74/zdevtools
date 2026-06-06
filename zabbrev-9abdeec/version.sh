#!/bin/sh
#
# version.sh -- Determine the version string for the build.
#
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

set -eu

branch=$(git rev-parse --abbrev-ref HEAD)

# If the branch is not 'master' AND a tag description exists, use it.
if [ "$branch" != "master" ] && git describe --tags >/dev/null 2>&1; then
  git describe --tags
else
  # For all other cases, generate the dev version string.
  commit_count=$(git rev-list HEAD --count)
  short_hash=$(git rev-parse --short HEAD)
  echo "dev-$commit_count-$short_hash"
fi
