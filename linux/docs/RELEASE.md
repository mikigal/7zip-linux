# Releases and the AUR packages

The Linux port is published as two AUR packages:

- `7zip-linux` builds from the source tarball of the release tag;
- `7zip-linux-bin` installs the `7zip-linux` package that the release
  workflow built with makepkg on Arch Linux (x86-64 baseline, Arch's
  default compiler flags), attached to the GitHub release.

Both install the same files. Their PKGBUILDs are in
`linux/packaging/aur/`. The AUR copies get the version and checksums from
`linux/packaging/aur/release.sh`.

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
   - pushes the PKGBUILD and `.SRCINFO` of both packages to the AUR.

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
