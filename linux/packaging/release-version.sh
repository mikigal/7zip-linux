#!/bin/bash
# Prints the version and the date of the last release in the AppStream
# metadata (its first <release>), for the package build scripts of
# linux/packaging/deb and linux/packaging/rpm. With VERSION, it also checks
# that VERSION is that release.
#
# Usage: linux/packaging/release-version.sh [VERSION]

set -euo pipefail

metainfo="$(cd "$(dirname "$0")" && pwd)/pl.mikigal.SevenZip.metainfo.xml"
if ! line=$(grep -m1 -o '<release version="[^"]*" date="[^"]*"' "$metainfo"); then
  echo "no <release version=... date=...> in $metainfo" >&2
  exit 1
fi
version=$(sed 's/.*version="\([^"]*\)".*/\1/' <<<"$line")
date=$(sed 's/.*date="\([^"]*\)".*/\1/' <<<"$line")
if [ -n "${1:-}" ] && [ "$1" != "$version" ]; then
  echo "the last release of the AppStream metadata is $version, not $1" >&2
  exit 1
fi
echo "$version $date"
