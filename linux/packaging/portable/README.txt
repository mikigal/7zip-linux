7-Zip for Linux, portable
=========================

The 7-Zip File Manager and the 7-Zip GUI for Linux: the original 7-Zip
design on Qt 6 and Wayland, in a dark theme.
https://github.com/mikigal/7zip-linux

This folder brings its own Qt and runs from where it is, on x86-64
distributions with glibc 2.36 or newer and a Wayland or X11 desktop:

  ./7zFM    the 7-Zip File Manager
  ./7zG     the 7-Zip GUI (7zG ARGUMENTS, as on Windows)

The settings are in ~/.config/7-Zip, as with the installed packages.

The portable folder and the AppImage have no 7-Zip menu in file managers
and no file associations; the packages for Arch Linux (AUR), Debian,
Ubuntu, Fedora and openSUSE have them.

Licenses: usr/share/doc/7zip-linux (7-Zip) and usr/share/doc/third-party
(Qt and the other libraries in usr/lib).
