#!/bin/bash
###
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2018-2023 SCANOSS.COM
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
###
#
# Package up the local binaries into deb or rpm packages for deployment on a server
#
if [ "$1" = "-h" ] || [ "$1" = "-help" ] ; then
  echo "$0 [-help] <package> <version>"
  echo "   Create a package of the ldb"
  echo "   <package> the package type (deb, rpm)"
  echo "   <version> version number of the package"
  exit 1
fi
if [[ -z "$1" || ( "$1" != "deb" && "$1" != "rpm" ) ]]  ; then
  echo "ERROR: Please provide a package type: deb or rpm"
  exit 1
fi
if [ -z "$2" ] ; then
  echo "ERROR: Please provide a package version"
  exit 1
fi

if [ "$1" = "deb" ] ; then
  # Add binaries
  mkdir -p dist/.debpkg/bin
  mkdir -p dist/.debpkg/lib
  cp ldb dist/.debpkg/bin/ldb
  chmod +x dist/.debpkg/bin/ldb
  cp libldb.so dist/.debpkg/lib/libldb.so
  # Add control file
  mkdir -p dist/.debpkg/DEBIAN
  cp scripts/debpkg/DEBIAN/control dist/.debpkg/DEBIAN/control
  sed -i 's/\LDB_VERSION/'"$2"'/g' dist/.debpkg/DEBIAN/control
  # Build package
  dpkg-deb -Zxz --root-owner-group --build ./dist/.debpkg/ .
fi

if [ "$1" = "rpm" ] ; then
  mkdir -p dist/.rpmpkg/
  cp ldb dist/.rpmpkg/ldb
  chmod +x ./dist/.rpmpkg/ldb
  cp libldb.so ./dist/.rpmpkg/libldb.so
  cp scripts/rpmpkg/ldb.spec dist/.rpmpkg/ldb.spec
  sed -i 's/\LDB_VERSION/'"$2"'/g' dist/.rpmpkg/ldb.spec
  rpmbuild -ba --build-in-place --define "_topdir $(pwd)/dist/rpm" dist/.rpmpkg/ldb.spec
fi