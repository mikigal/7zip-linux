#!/bin/bash
# Builds the Fedora or openSUSE package of the Linux port for the system it
# runs on; the Packages workflow (.github/workflows/packages.yml) runs it in
# a container of each distribution. It makes a tarball of the source tree
# (without build/ and .git/) and runs rpmbuild with 7zip-linux.spec, with
# the unit tests. See linux/docs/RELEASE.md.
#
# Usage: linux/packaging/rpm/build.sh [--deps] OUTDIR [VERSION]
#
# VERSION (26.05.1) must be the last release of the AppStream metadata,
# which is the default. The release names the distribution: 1.fc44, and
# 1.opensuse_tumbleweed where the system has no %dist. --deps first
# installs the packaging tools and the BuildRequires of the spec, with dnf
# or zypper (as root).
#
# OUTDIR gets 7zip-linux-VERSION-RELEASE.ARCH.rpm.

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
read -r version _ < <("$here/../release-version.sh" "${2:-}")
spec="$here/7zip-linux.spec"

# install PACKAGES...: with the package manager of the system
install() {
  if command -v dnf >/dev/null; then
    dnf install -y "$@"
  else
    zypper --non-interactive install --no-recommends "$@"
  fi
}

if [ $deps = 1 ]; then
  install rpm-build tar gzip
fi
dist=$(rpm --eval '%{?dist}')
if [ -z "$dist" ]; then
  . /etc/os-release
  dist=".${ID//-/_}"
fi
defines=(--define "z7_version $version" --define "dist $dist")
if [ $deps = 1 ]; then
  mapfile -t buildrequires < <(rpmspec -q --buildrequires "${defines[@]}" "$spec")
  install "${buildrequires[@]}"
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/SOURCES"
tar -C "$root" --exclude=./build --exclude=./.git --transform "s,^\.,7zip-linux-$version," \
  -czf "$work/SOURCES/7zip-linux-$version.tar.gz" .
rpmbuild -bb "${defines[@]}" --define "_topdir $work" "$spec"
for rpm in "$work"/RPMS/*/7zip-linux-"$version"-*.rpm; do
  cp "$rpm" "$out/"
  echo "$out/$(basename "$rpm")"
done
