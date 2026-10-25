#!/bin/bash
# Runs the portable releases of linux/packaging/portable/build.sh on the
# system it runs on: installs only the libraries of a desktop system that
# they do not bring (OpenGL, the Wayland and X11 client libraries,
# fontconfig and a font), checks that every program and plugin finds its
# libraries and uses the Qt of the release, and starts 7zFM (offscreen)
# from the tar.gz and from the AppImage. The Packages workflow
# (.github/workflows/packages.yml) runs it as root in fresh containers of
# other distributions than the one that built them.
#
# Usage: linux/packaging/portable/run-test.sh FILE...   (.tar.gz, .AppImage)

set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 FILE..." >&2
  exit 2
fi

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

. /etc/os-release
case " $ID ${ID_LIKE:-} " in
  *" arch "*)
    pacman -Syu --noconfirm --needed mesa libglvnd fontconfig ttf-dejavu \
      wayland libx11 libxcb libsm libice tar gzip
    ;;
  *" fedora "*)
    dnf install -y --setopt=install_weak_deps=False mesa-libGL mesa-libEGL mesa-libgbm \
      fontconfig dejavu-sans-fonts libwayland-client libX11 libX11-xcb libxcb \
      libSM libICE tar gzip
    ;;
  *" debian "*)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends libgl1 libegl1 libgbm1 \
      libfontconfig1 fonts-dejavu-core libwayland-client0 libx11-6 libx11-xcb1 \
      libxcb1 libsm6 libice6
    ;;
  *)
    fail "no list of desktop libraries for $ID"
    ;;
esac

# a private home and no session bus
home=$(mktemp -d)
export HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_DATA_HOME="$home/data"
export XDG_CACHE_HOME="$home/cache" XDG_RUNTIME_DIR="$home/run"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$home/no-bus"
unset DISPLAY WAYLAND_DISPLAY QT_PLUGIN_PATH
mkdir -m 700 "$XDG_RUNTIME_DIR"

# check DIR: the programs, libraries and plugins of the folder DIR find
# their libraries, and the programs use the Qt of DIR
check() {
  local elf missing=0
  for elf in "$1"/usr/bin/7zFM "$1"/usr/bin/7zG "$1"/usr/lib/*.so* "$1"/usr/plugins/*/*.so; do
    if ldd "$elf" | grep 'not found'; then
      echo "$elf: libraries not found" >&2
      missing=1
    fi
  done
  [ $missing = 0 ] || fail "libraries not found in $1"
  for program in 7zFM 7zG; do
    qt=$(ldd "$1/usr/bin/$program" | awk '$1 == "libQt6Core.so.6" { print $3 }')
    [ -n "$qt" ] && [ "$(realpath "$qt")" = "$(realpath "$1/usr/lib/libQt6Core.so.6")" ] \
      || fail "$program does not use the Qt of $1 ($qt)"
  done
  [ -e "$1/usr/bin/Lang/pl.txt" ] || fail "no Lang in $1/usr/bin"
}

# start COMMAND...: 7zFM runs until the timeout ends it (124)
start() {
  local status=0
  QT_QPA_PLATFORM=offscreen timeout --kill-after=5 10 "$@" || status=$?
  [ $status = 124 ] || fail "$* exited with $status"
}

for file in "$@"; do
  file=$(realpath "$file")
  dir=$(mktemp -d)
  case $file in
    *.tar.gz)
      tar -C "$dir" -xzf "$file"
      folder=("$dir"/*)
      check "${folder[0]}"
      start "${folder[0]}/7zFM" "$home"
      ;;
    *.AppImage)
      chmod +x "$file"
      (cd "$dir" && "$file" --appimage-extract > /dev/null)
      check "$dir/squashfs-root"
      # the runtime of the AppImage, without FUSE (containers)
      export APPIMAGE_EXTRACT_AND_RUN=1
      start "$file" "$home"
      unset APPIMAGE_EXTRACT_AND_RUN
      ;;
    *)
      fail "not a .tar.gz or .AppImage: $file"
      ;;
  esac
  echo "OK: $(basename "$file") on $PRETTY_NAME"
done
