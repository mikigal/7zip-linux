#!/bin/bash
# Makes the AUR release of the Linux port: builds the 7zip-linux package with
# makepkg from the source tarball of the tag vVERSION (the release asset that
# 7zip-linux-bin installs), checks that 7zip-linux-bin installs the same
# files, and writes both AUR packages (PKGBUILD, .SRCINFO) with the version
# and the checksums of the release. The release workflow
# (.github/workflows/release.yml) runs it; see linux/docs/RELEASE.md.
#
# Usage: linux/packaging/aur/release.sh VERSION OUTDIR [SOURCE_TARBALL]
#
# Without SOURCE_TARBALL it downloads the tarball of the tag from GitHub,
# the file that the 7zip-linux package downloads. For a local test, make one
# from a commit:
#   git archive --prefix=7zip-linux-VERSION/ -o FILE.tar.gz HEAD
#
# OUTDIR gets:
#   7zip-linux-VERSION-x86_64.pkg.tar.zst  the release asset
#   aur/7zip-linux/, aur/7zip-linux-bin/   the AUR packages
#
# makepkg needs a user other than root and the build dependencies of the
# PKGBUILD installed.

set -euo pipefail

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "usage: $0 VERSION OUTDIR [SOURCE_TARBALL]" >&2
  exit 2
fi
version=$1
# pkgver: no hyphens; the tag is v$version
if ! [[ $version =~ ^[0-9]+(\.[0-9]+)+$ ]]; then
  echo "bad version: $version (expected for example 26.05.1)" >&2
  exit 2
fi
out=$(mkdir -p "$2" && cd "$2" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
url=https://github.com/mikigal/7zip-linux
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/src"
tarball="$work/src/7zip-linux-$version.tar.gz"
if [ $# -eq 3 ]; then
  cp "$3" "$tarball"
else
  curl -fsSL -o "$tarball" "$url/archive/refs/tags/v$version.tar.gz"
fi

# the release names its version (RELEASE.md, "Making a release")
for name in 7zip-linux 7zip-linux-bin; do
  if ! grep -qx "pkgver=$version" "$here/$name/PKGBUILD"; then
    echo "$name/PKGBUILD: pkgver is not $version" >&2
    exit 1
  fi
done
if ! tar -xzOf "$tarball" "7zip-linux-$version/linux/packaging/pl.mikigal.SevenZip.metainfo.xml" \
    | grep -q "<release version=\"$version\""; then
  echo "the AppStream metadata of the release has no <release version=\"$version\">" >&2
  exit 1
fi

rm -rf "$out/aur"
mkdir -p "$out/aur"
cp -r "$here/7zip-linux" "$here/7zip-linux-bin" "$out/aur/"

# set_package NAME FILE: pkgver, pkgrel 1 and the checksum of FILE
set_package() {
  local sum
  sum=$(sha256sum "$2" | cut -d' ' -f1)
  sed -i -e "s/^pkgver=.*/pkgver=$version/" -e "s/^pkgrel=.*/pkgrel=1/" \
    -e "s/^sha256sums=.*/sha256sums=('$sum')/" "$out/aur/$1/PKGBUILD"
}

# build NAME: makepkg on a copy of the AUR package, with the sources of
# $work/src; prints the package file
build() {
  mkdir -p "$work/$1" "$work/pkg"
  cp "$out/aur/$1/PKGBUILD" "$work/$1/"
  (cd "$work/$1" && SRCDEST="$work/src" BUILDDIR="$work/$1/build" PKGDEST="$work/pkg" \
    PKGEXT=.pkg.tar.zst makepkg --noconfirm --cleanbuild >&2)
  echo "$work/pkg/$1-$version-1-x86_64.pkg.tar.zst"
}

# the files of a package, without its metadata
files() {
  bsdtar -tf "$1" | grep -v '^\.' | sort
}

set_package 7zip-linux "$tarball"
pkg=$(build 7zip-linux)
asset="7zip-linux-$version-x86_64.pkg.tar.zst"
cp "$pkg" "$out/$asset"
cp "$pkg" "$work/src/$asset"

set_package 7zip-linux-bin "$out/$asset"
bin=$(build 7zip-linux-bin)
if ! diff <(files "$pkg") <(files "$bin") >&2; then
  echo "7zip-linux-bin installs other files than 7zip-linux" >&2
  exit 1
fi

for name in 7zip-linux 7zip-linux-bin; do
  (cd "$out/aur/$name" && makepkg --printsrcinfo > .SRCINFO)
done
echo "$out/$asset"
