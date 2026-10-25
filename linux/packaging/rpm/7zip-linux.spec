# The Fedora and openSUSE package of the Linux port. linux/packaging/rpm/build.sh
# builds it from a tarball of the source tree, with the version of the release
# (z7_version) and the distribution in the release (dist); see
# linux/docs/RELEASE.md.

Name:           7zip-linux
Version:        %{z7_version}
Release:        1%{?dist}
Summary:        7-Zip File Manager and GUI for Linux
License:        LGPL-2.1-or-later AND LicenseRef-UnRAR AND BSD-3-Clause AND BSD-2-Clause
URL:            https://github.com/mikigal/7zip-linux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.25
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  python3
BuildRequires:  pkgconfig
BuildRequires:  shared-mime-info
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Svg)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(KF6KIO)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(libnemo-extension)
BuildRequires:  pkgconfig(thunarx-3)
BuildRequires:  pkgconfig(libnautilus-extension-4)

Requires:       hicolor-icon-theme
# the Qt plugins, which no library names: the Wayland platform and the SVG
# icon engine (the toolbar)
%if 0%{?suse_version}
Requires:       qt6-wayland
Requires:       libQt6Svg6
%else
Requires:       qt6-qtwayland
Requires:       qt6-qtsvg
%endif
Suggests:       dolphin
Suggests:       nemo
Suggests:       thunar
Suggests:       nautilus

# the file manager plugins: their libraries come with the file managers
# (Suggests), as the optdepends of the AUR packages; they are no libraries
# for other packages
%global __requires_exclude_from ^%{_libdir}/(qt6/plugins/kf6/kfileitemaction|nemo|thunarx-3|nautilus)/.*$
%global __provides_exclude_from %{__requires_exclude_from}

%description
The original 7-Zip design on Qt 6 and Wayland, in a dark theme: the 7-Zip
File Manager (7zFM), the 7-Zip GUI (7zG) and the 7-Zip menu in Dolphin and
other KDE programs, Nemo, Thunar and Nautilus.

%prep
%autosetup

%build
%if %{defined set_build_flags}
%set_build_flags
%else
export CFLAGS="%{optflags}" CXXFLAGS="%{optflags}"
%endif
# no upstream 7zz test oracle; new compiler warnings must not stop a build
cmake -S linux -B build \
  -DCMAKE_BUILD_TYPE=None \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_INSTALL_LIBDIR=%{_lib} \
  -DZ7_BUILD_ORACLE=OFF \
  -DZ7_WERROR=OFF
cmake --build build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build

%check
ctest --test-dir build -L unit --output-on-failure

%files
%license DOC/License.txt DOC/copying.txt DOC/unRarLicense.txt
%{_bindir}/7zFM
%{_bindir}/7zG
%{_libdir}/7zip-linux/
%{_libdir}/qt6/plugins/kf6/kfileitemaction/sevenzipfileitemaction.so
%{_libdir}/nemo/extensions-*/libnemo-sevenzip.so
%{_libdir}/thunarx-3/thunar-sevenzip.so
%{_libdir}/nautilus/extensions-4/libnautilus-sevenzip.so
%{_datadir}/applications/pl.mikigal.SevenZip.desktop
%{_datadir}/applications/pl.mikigal.SevenZip.Gui.desktop
%{_datadir}/icons/hicolor/scalable/apps/pl.mikigal.SevenZip.svg
%{_datadir}/mime/packages/pl.mikigal.SevenZip.xml
%{_datadir}/metainfo/pl.mikigal.SevenZip.metainfo.xml
