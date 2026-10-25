#!/bin/bash
# Builds the portable releases of the Linux port, which bring their own Qt:
# an AppImage, and a tar.gz with the same folder, for the distributions that
# have no package of their own. The Packages workflow
# (.github/workflows/packages.yml) runs it in a Debian 12 container, so that
# they need glibc 2.36 only. Qt is the release of the Qt Company
# (aqtinstall); linuxdeploy bundles the libraries that it and the programs
# need, except those that every desktop system has (the AppImage
# excludelist: glibc, libstdc++, OpenGL, the Wayland and X11 client
# libraries, fontconfig, ...). See linux/docs/RELEASE.md.
#
# Usage: linux/packaging/portable/build.sh [--deps] OUTDIR [VERSION]
#
# VERSION (26.05.1) must be the last release of the AppStream metadata,
# which is the default. --deps first installs the build tools, Qt (in
# /opt/qt) and linuxdeploy (in /opt/linuxdeploy), as root on Debian 12.
# Without it, QT_PREFIX names the Qt folder (with lib/ and plugins/) and
# LINUXDEPLOY linuxdeploy-x86_64.AppImage.
#
# OUTDIR gets 7zip-linux-VERSION-x86_64.AppImage and
# 7zip-linux-VERSION-x86_64.tar.gz.

set -euo pipefail

qt_version=6.11.3
aqt_version=3.3.0
linuxdeploy_url=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
linuxdeploy_sha256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d

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

if [ $deps = 1 ]; then
  export DEBIAN_FRONTEND=noninteractive
  apt-get update
  # the build, Qt and linuxdeploy, and the libraries that Qt needs and
  # linuxdeploy bundles; no shared-mime-info: the portable releases
  # associate no types, and Debian 12's names some of them otherwise
  # (unit_packaging skips the check without it)
  apt-get install -y --no-install-recommends \
    build-essential cmake pkgconf python3 python3-venv \
    ca-certificates curl patchelf file \
    libgl-dev libegl-dev libvulkan-dev libxkbcommon-dev libfontconfig-dev \
    libglib2.0-0 libdbus-1-3 libxkbcommon-x11-0 libwayland-cursor0 libwayland-egl1 \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1
  python3 -m venv /opt/aqt
  /opt/aqt/bin/pip install --quiet "aqtinstall==$aqt_version"
  /opt/aqt/bin/aqt install-qt linux desktop "$qt_version" linux_gcc_64 --outputdir /opt/qt
  mkdir -p /opt/linuxdeploy
  curl -fsSL -o /opt/linuxdeploy/linuxdeploy-x86_64.AppImage "$linuxdeploy_url"
  echo "$linuxdeploy_sha256  /opt/linuxdeploy/linuxdeploy-x86_64.AppImage" | sha256sum -c -
  chmod +x /opt/linuxdeploy/linuxdeploy-x86_64.AppImage
  QT_PREFIX=/opt/qt/$qt_version/gcc_64
  LINUXDEPLOY=/opt/linuxdeploy/linuxdeploy-x86_64.AppImage
fi
: "${QT_PREFIX:?the Qt folder (or --deps)}"
: "${LINUXDEPLOY:?linuxdeploy-x86_64.AppImage (or --deps)}"
# linuxdeploy without FUSE (containers)
export APPIMAGE_EXTRACT_AND_RUN=1

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# no upstream 7zz test oracle; new compiler warnings must not stop a build
cmake -S "$root/linux" -B "$work/build" \
  -DCMAKE_BUILD_TYPE=None -DCMAKE_C_FLAGS=-O2 -DCMAKE_CXX_FLAGS=-O2 \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DZ7_BUILD_ORACLE=OFF -DZ7_WERROR=OFF
cmake --build "$work/build" -j"$(nproc)"
ctest --test-dir "$work/build" -L unit --output-on-failure
DESTDIR="$work/stage" cmake --install "$work/build"

# The AppDir: the programs with Lang/ in usr/bin (they find each other and
# the language files in their folder), the Qt plugins that they load in
# usr/plugins (qt.conf), the libraries in usr/lib (linuxdeploy). No
# 7z-context-menu: no file manager loads the plugins of a portable folder.
app="$work/AppDir"
programs="$work/stage/usr/lib/7zip-linux"
mkdir -p "$app/usr/bin" "$app/usr/plugins" "$app/usr/share/doc/7zip-linux"
cp -a "$programs/7zFM" "$programs/7zG" "$programs/Lang" "$app/usr/bin/"
for dir in applications icons/hicolor/scalable/apps; do
  mkdir -p "$app/usr/share/$dir"
done
cp "$work/stage/usr/share/applications/pl.mikigal.SevenZip.desktop" "$app/usr/share/applications/"
cp "$work/stage/usr/share/icons/hicolor/scalable/apps/pl.mikigal.SevenZip.svg" \
  "$app/usr/share/icons/hicolor/scalable/apps/"
cp "$root/DOC/License.txt" "$root/DOC/copying.txt" "$root/DOC/unRarLicense.txt" \
  "$app/usr/share/doc/7zip-linux/"
# the platforms (Wayland, X11, and offscreen for scripts and tests) with
# their integrations, and the SVG icon engine of the toolbar
(cd "$QT_PREFIX/plugins" && cp -a --parents \
  platforms/libqwayland*.so platforms/libqxcb.so platforms/libqoffscreen.so \
  wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client \
  xcbglintegrations iconengines/libqsvgicon.so imageformats/libqsvg.so \
  "$app/usr/plugins/")
printf '[Paths]\nPrefix = ..\nPlugins = plugins\n' > "$app/usr/bin/qt.conf"

# launcher NAME: a script next to usr/ that starts usr/bin/NAME with the Qt
# plugins of the folder only (QT_PLUGIN_PATH may name those of another Qt)
launcher() {
  cat <<EOF
#!/bin/sh
here=\$(dirname "\$(readlink -f "\$0")")
unset QT_PLUGIN_PATH
exec "\$here/usr/bin/$1" "\$@"
EOF
}
launcher 7zFM > "$work/AppRun"
chmod +x "$work/AppRun"

elfs=("$app/usr/bin/7zFM" "$app/usr/bin/7zG")
mapfile -t -O 2 elfs < <(find "$app/usr/plugins" -name '*.so')
deploy=()
for elf in "${elfs[@]}"; do
  deploy+=(--deploy-deps-only "$elf")
done
LD_LIBRARY_PATH="$QT_PREFIX/lib" "$LINUXDEPLOY" --appdir "$app" \
  --desktop-file "$app/usr/share/applications/pl.mikigal.SevenZip.desktop" \
  --icon-file "$app/usr/share/icons/hicolor/scalable/apps/pl.mikigal.SevenZip.svg" \
  --custom-apprun "$work/AppRun" "${deploy[@]}"
# the programs and plugins find the libraries in usr/lib (mode 755, as the
# Qt ones; Debian's have 644)
chmod 755 "$app"/usr/lib/*.so*
for elf in "${elfs[@]}"; do
  case $elf in
    "$app"/usr/bin/*) patchelf --set-rpath '$ORIGIN/../lib' "$elf" ;;
    *) patchelf --set-rpath '$ORIGIN/../../lib' "$elf" ;;
  esac
done

# where the libraries come from, with the licenses of the Debian ones
doc="$app/usr/share/doc/third-party"
mkdir -p "$doc/qt"
cp /usr/share/common-licenses/LGPL-3 /usr/share/common-licenses/GPL-3 "$doc/qt/"
{
  echo "The libraries in usr/lib and the plugins in usr/plugins:"
  echo
  echo "- usr/plugins: Qt $qt_version, LGPL-3.0-only (qt/LGPL-3, qt/GPL-3), source at"
  echo "  https://download.qt.io/official_releases/qt/"
  for lib in "$app"/usr/lib/*.so*; do
    name=$(basename "$lib")
    if [ -e "$QT_PREFIX/lib/$name" ]; then
      case $name in
        libicu*) echo "- $name: ICU of the Qt $qt_version release, Unicode License," \
                      "https://github.com/unicode-org/icu/blob/main/LICENSE" ;;
        *) echo "- $name: Qt $qt_version, LGPL-3.0-only (qt/LGPL-3, qt/GPL-3)" ;;
      esac
    else
      pkg=$(dpkg -S "*/$name" | awk -F: 'NR == 1 { print $1 }')
      mkdir -p "$doc/$pkg"
      cp -L "/usr/share/doc/$pkg/copyright" "$doc/$pkg/"
      echo "- $name: Debian 12 package $pkg ($pkg/copyright)"
    fi
  done
} > "$doc/README.txt"

name="7zip-linux-$version-x86_64"
# the tar.gz: the folder with launchers instead of the AppImage files
cp -a "$app" "$work/$name"
rm -f "$work/$name/AppRun" "$work/$name/.DirIcon" "$work/$name"/*.desktop "$work/$name"/*.svg
for program in 7zFM 7zG; do
  launcher $program > "$work/$name/$program"
  chmod +x "$work/$name/$program"
done
cp "$here/README.txt" "$work/$name/"
tar -C "$work" --owner=0 --group=0 --numeric-owner -czf "$out/$name.tar.gz" "$name"
echo "$out/$name.tar.gz"

LINUXDEPLOY_OUTPUT_VERSION=$version LDAI_OUTPUT="$out/$name.AppImage" \
  LD_LIBRARY_PATH="$QT_PREFIX/lib" "$LINUXDEPLOY" --appdir "$app" --output appimage
echo "$out/$name.AppImage"
