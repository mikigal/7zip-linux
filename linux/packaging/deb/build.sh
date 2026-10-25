#!/bin/bash
# Builds the Debian or Ubuntu package of the Linux port for the system it
# runs on; the Packages workflow (.github/workflows/packages.yml) runs it in
# a container of each distribution. It copies the source tree (without
# build/ and .git/) with linux/packaging/deb/debian at its top, writes
# debian/changelog and runs dpkg-buildpackage, with the unit tests. See
# linux/docs/RELEASE.md.
#
# Usage: linux/packaging/deb/build.sh [--deps] OUTDIR [VERSION]
#
# VERSION (26.05.1) must be the last release of the AppStream metadata,
# which is the default. The package version names the distribution:
# 26.05.1-1~debian13, 26.05.1-1~ubuntu26.04. --deps first installs the
# packaging tools and the build dependencies of debian/control (as root).
#
# OUTDIR gets 7zip-linux_PACKAGEVERSION_ARCH.deb.

set -euo pipefail

deps=0
if [ "${1:-}" = --deps ]; then
  deps=1
  shift
fi
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "usage: $0 [--deps] OUTDIR [VERSION]" >&2
  exit 2
fi
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=$(mkdir -p "$1" && cd "$1" && pwd)
read -r version date < <("$here/../release-version.sh" "${2:-}")
. /etc/os-release
pkgversion="$version-1~$ID$VERSION_ID"
export DEBIAN_FRONTEND=noninteractive

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
src="$work/7zip-linux-$version"
mkdir "$src"
tar -C "$root" --exclude=./build --exclude=./.git -cf - . | tar -C "$src" -xf -
cp -r "$here/debian" "$src/"
cat > "$src/debian/changelog" <<EOF
7zip-linux ($pkgversion) $VERSION_CODENAME; urgency=medium

  * 7-Zip for Linux $version.

 -- mikigal <mikigal.priv@gmail.com>  $(LC_ALL=C date -u -R -d "$date")
EOF

if [ $deps = 1 ]; then
  apt-get update
  apt-get install -y --no-install-recommends dpkg-dev debhelper
  (cd "$src" && apt-get build-dep -y --no-install-recommends ./)
fi

# no -dbgsym package
(cd "$src" && DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS:+$DEB_BUILD_OPTIONS }noddebs" dpkg-buildpackage -b -us -uc)
deb="7zip-linux_${pkgversion}_$(dpkg --print-architecture).deb"
cp "$work/$deb" "$out/"
echo "$out/$deb"
