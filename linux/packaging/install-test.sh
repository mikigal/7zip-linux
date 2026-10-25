#!/bin/bash
# Installs a package of the Linux port (.deb or .rpm) with the package
# manager of the system, which brings its dependencies (without the
# recommended and suggested ones) from the distribution, and checks that it
# works there: the programs find every library, Qt has the plugins that no
# library dependency brings (the Wayland and offscreen platforms, the SVG
# icon engine), 7z-context-menu prints the menu for a file and 7zFM starts
# (offscreen). The Packages workflow (.github/workflows/packages.yml) runs it
# as root in a fresh container of the distribution that the package was
# built for.
#
# Usage: linux/packaging/install-test.sh PACKAGE

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 PACKAGE" >&2
  exit 2
fi
pkg=$(realpath "$1")

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

case $pkg in
  *.deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends "$pkg"
    ;;
  *.rpm)
    if command -v dnf >/dev/null; then
      dnf install -y --setopt=install_weak_deps=False "$pkg"
    else
      zypper --non-interactive install --no-recommends --allow-unsigned-rpm "$pkg"
    fi
    ;;
  *)
    fail "not a .deb or .rpm: $pkg"
    ;;
esac

programs=$(dirname "$(readlink -f /usr/bin/7zFM)")
for program in /usr/bin/7zFM /usr/bin/7zG "$programs/7z-context-menu"; do
  [ -x "$program" ] || fail "no $program"
  if ldd "$program" | grep 'not found'; then
    fail "$program: libraries not found"
  fi
done

# qt_plugin PATTERN: a Qt plugin that no library dependency brings
qt_plugin() {
  local path found=()
  for path in /usr/lib{,64}/qt6/plugins/$1 /usr/lib/*/qt6/plugins/$1; do
    if [ -e "$path" ]; then
      found+=("$path")
    fi
  done
  [ ${#found[@]} -gt 0 ] || fail "no Qt plugin $1"
  echo "Qt plugin: ${found[*]}"
}
# libqwayland-generic.so before Qt 6.10, libqwayland.so since
qt_plugin "platforms/libqwayland*.so"
qt_plugin "platforms/libqoffscreen.so"
# the toolbar icons
qt_plugin "iconengines/libqsvgicon.so"

# a private home and no session bus
home=$(mktemp -d)
export HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_DATA_HOME="$home/data"
export XDG_CACHE_HOME="$home/cache" XDG_RUNTIME_DIR="$home/run"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$home/no-bus"
unset DISPLAY WAYLAND_DISPLAY
mkdir -m 700 "$XDG_RUNTIME_DIR"

# the menu that the GTK file managers show: NUL-terminated fields
# (linux/shell/ShellMenu.h), items that start 7zG or 7zFM
echo test > "$home/file.txt"
menu=$("$programs/7z-context-menu" "$home/file.txt" | tr '\0' '\n')
echo "$menu"
[ "$(head -n1 <<<"$menu")" = "7-Zip menu 1" ] || fail "7z-context-menu printed no menu"
grep -qx "$programs/7zG" <<<"$menu" || fail "no menu item starts $programs/7zG"

# 7zFM runs until the timeout ends it (124)
status=0
QT_QPA_PLATFORM=offscreen timeout 10 /usr/bin/7zFM "$home" || status=$?
[ $status = 124 ] || fail "7zFM exited with $status"

echo "OK: $(basename "$pkg")"
