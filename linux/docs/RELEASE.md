# Releases and packages

The Linux port is published as two AUR packages:

- `7zip-linux` builds from the source tarball of the release tag;
- `7zip-linux-bin` installs the `7zip-linux` package that the release
  workflow built with makepkg on Arch Linux (x86-64 baseline, Arch's
  default compiler flags), attached to the GitHub release.

Both install the same files. Their PKGBUILDs are in
`linux/packaging/aur/`. The AUR copies get the version and checksums from
`linux/packaging/aur/release.sh`.

The GitHub release also has packages for Debian 13, Ubuntu 26.04 LTS,
Fedora 43 and 44 and openSUSE Tumbleweed (see "Debian, Ubuntu, Fedora and
openSUSE" below), and portable releases with their own Qt for the other
distributions: an AppImage and a tar.gz (see "Portable releases" below).

## Version numbers

`VERSION` is the upstream 7-Zip version plus the release number of the
port: `26.05.1` is the first port release on top of 7-Zip 26.05, `26.05.2`
the next one, and `26.06.1` the first one after merging 7-Zip 26.06. The git
tag is `vVERSION`. A packaging fix is a new port release too: the release
workflow always sets `pkgrel=1`.

## One-time setup

Only the owner of the GitHub repository and of the AUR account can do this.

1. Make an account on [aur.archlinux.org](https://aur.archlinux.org/register).
2. Make an SSH key for the release workflow, without a passphrase:

   ```sh
   ssh-keygen -t ed25519 -N "" -C "7zip-linux release" -f ~/.ssh/aur-7zip-linux
   ```

3. In the AUR, My Account, add the public key (`~/.ssh/aur-7zip-linux.pub`)
   under "SSH Public Key".
4. Give the private key to GitHub as the repository secret
   `AUR_SSH_PRIVATE_KEY` (Settings, Secrets and variables, Actions), or:

   ```sh
   gh secret set AUR_SSH_PRIVATE_KEY --repo mikigal/7zip-linux < ~/.ssh/aur-7zip-linux
   ```

The first push makes the AUR packages; they belong to the account of the
key. Without the secret, the workflow still builds the package and attaches
it to the release, and prints the PKGBUILDs instead of pushing them.

## Making a release

1. Set the version in both PKGBUILDs (`pkgver=`) and add it to the AppStream
   metadata (`linux/packaging/pl.mikigal.SevenZip.metainfo.xml`,
   `<release version="VERSION" date="YYYY-MM-DD"/>` first in `<releases>`).
   `release.sh` stops when either is missing.
2. Run `linux/tools/check.sh`, commit, and try the release locally (see
   below).
3. Push `main` and the tag:

   ```sh
   git tag -a v26.05.1 -m "7-Zip for Linux 26.05.1"
   git push origin main v26.05.1
   ```

4. Publish a GitHub release for the tag, on the web or with
   `gh release create v26.05.1 --title "7-Zip for Linux 26.05.1" --notes "..."`.
5. The workflow `.github/workflows/release.yml` (Actions tab) then:
   - builds `7zip-linux` with makepkg in an `archlinux:base-devel`
     container, from the GitHub tarball of the tag (the same file the AUR
     package downloads), including its `check()` (the unit tests);
   - builds `7zip-linux-bin` from that package and checks that both have
     the same files;
   - attaches `7zip-linux-VERSION-x86_64.pkg.tar.zst` to the release;
   - pushes the PKGBUILD and `.SRCINFO` of both packages to the AUR;
   - builds and tests the Debian, Ubuntu, Fedora and openSUSE packages and
     the portable releases (`.github/workflows/packages.yml`) from the tag
     and attaches them to the release.

   To run it again for a published release (after a failed AUR push, for
   example), start it by hand with the tag (Actions, Release, Run workflow).

The package attached to the release can also be installed directly:
`sudo pacman -U 7zip-linux-VERSION-x86_64.pkg.tar.zst`.

## Trying a release locally

`release.sh` does what the workflow does, from a source tarball made from a
commit instead of the GitHub tarball of a tag. It needs the build
dependencies of the PKGBUILD (`cmake python kio nemo thunar
libnautilus-extension`) and runs makepkg, so not as root:

```sh
git archive --prefix=7zip-linux-26.05.1/ -o /tmp/7zip-linux.tar.gz HEAD
linux/packaging/aur/release.sh 26.05.1 /tmp/7zip-linux-release /tmp/7zip-linux.tar.gz
```

`/tmp/7zip-linux-release` then has the package and `aur/` with the two AUR
packages as the workflow would push them. Locally, makepkg uses
`/etc/makepkg.conf`, which on CachyOS builds for the local CPU; the release
workflow builds for the x86-64 baseline.

## Debian, Ubuntu, Fedora and openSUSE

`.github/workflows/packages.yml` builds one package for each distribution
of its matrix, each in a container of that distribution (Docker on the
GitHub runner):

- `linux/packaging/deb/build.sh` copies `linux/packaging/deb/debian` to the
  top of a copy of the source tree, writes `debian/changelog` and runs
  `dpkg-buildpackage`; `linux/packaging/rpm/build.sh` runs `rpmbuild` with
  `linux/packaging/rpm/7zip-linux.spec`, whose BuildRequires use the
  `cmake()` and `pkgconfig()` names that Fedora and openSUSE share. Both
  run the unit tests; `--deps` first installs the build dependencies.
- `linux/packaging/install-test.sh` then installs the package in a fresh
  container of the same distribution with apt, dnf or zypper (without
  recommended packages), checks that the programs find their libraries and
  that Qt has the plugins that no library dependency brings (the Wayland
  and offscreen platforms, the SVG icon engine of the toolbar), prints the
  7-Zip menu with `7z-context-menu` and starts 7zFM offscreen.

The version is the last release of the AppStream metadata
(`linux/packaging/release-version.sh`; for a release, the tag must be that
version), and the package version names the distribution:
`7zip-linux_26.05.1-1~debian13_amd64.deb`,
`7zip-linux-26.05.1-1.fc44.x86_64.rpm`,
`7zip-linux-26.05.1-1.opensuse_tumbleweed.x86_64.rpm`. As in the PKGBUILDs,
the packages have the plugins of Dolphin (KIO), Nemo, Thunar and Nautilus,
but their libraries are no dependencies: they come with the file managers
(Suggests).

The workflow runs only for a release (the release workflow calls it with
the tag) and by hand: after a change to the packaging, start it for the
branch before tagging (Actions, Packages, Run workflow, or
`gh workflow run Packages --ref main`); it builds the version of the
AppStream metadata. A distribution is one line of its matrix: the
container image and `deb` or `rpm`.

The packages install with their dependencies from the distribution:

```sh
sudo apt install ./7zip-linux_26.05.1-1~debian13_amd64.deb
sudo dnf install ./7zip-linux-26.05.1-1.fc44.x86_64.rpm
sudo zypper install --allow-unsigned-rpm ./7zip-linux-26.05.1-1.opensuse_tumbleweed.x86_64.rpm
```

They are not signed and not in a repository, so a new version is installed
the same way.

To try a distribution locally, run the scripts in its container, for
example with podman:

```sh
podman run --rm -v "$PWD:/src:z" -w /src debian:13 linux/packaging/deb/build.sh --deps build/packages
podman run --rm -v "$PWD:/src:z" -w /src debian:13 linux/packaging/install-test.sh build/packages/7zip-linux_26.05.1-1~debian13_amd64.deb
```

## Portable releases

For the distributions without a package, the release has
`7zip-linux-VERSION-x86_64.AppImage` and `7zip-linux-VERSION-x86_64.tar.gz`.
Both bring their own Qt and run without installation on x86-64 with glibc
2.36 or newer (Debian 12, Ubuntu 24.04 LTS and newer) in a Wayland or X11
desktop. They have no 7-Zip menu in file managers and no file associations:
a file manager cannot load the plugins of a portable folder.

`linux/packaging/portable/build.sh` makes both in a Debian 12 container
(the job `portable` of the Packages workflow):

- it builds with the Qt release of the Qt Company (`aqtinstall`, the
  version in the script) and runs the unit tests;
- the AppDir has 7zFM, 7zG and `Lang/` in `usr/bin` (the programs find each
  other and the language files in their folder), the Qt plugins that they
  load in `usr/plugins` (the Wayland, X11 and offscreen platforms with their
  integrations, the SVG icon engine; `usr/bin/qt.conf`), and the libraries
  in `usr/lib`, which linuxdeploy (pinned, with its SHA-256) bundles,
  except those of the AppImage excludelist that every desktop system has
  (glibc, libstdc++, OpenGL, the Wayland and X11 client libraries,
  fontconfig, ...);
- `usr/share/doc/third-party` lists where each library comes from, with the
  LGPL of Qt and the copyright files of the Debian packages;
- the tar.gz is that folder with the scripts `7zFM` and `7zG` (and
  `README.txt` of `linux/packaging/portable`); the AppImage starts 7zFM.
  Both scripts unset `QT_PLUGIN_PATH`, which could name the plugins of
  another Qt.

`linux/packaging/portable/run-test.sh` then runs both in fresh containers
of Ubuntu 24.04, Fedora 44 and Arch Linux with only the desktop libraries
of the excludelist installed: every program, library and plugin finds its
libraries, the programs use the bundled Qt, and 7zFM starts offscreen from
the tar.gz and from the AppImage (with `APPIMAGE_EXTRACT_AND_RUN`, as
containers have no FUSE).

Users run them as they are:

```sh
chmod +x 7zip-linux-26.05.1-x86_64.AppImage
./7zip-linux-26.05.1-x86_64.AppImage
tar -xzf 7zip-linux-26.05.1-x86_64.tar.gz
7zip-linux-26.05.1-x86_64/7zFM
```
