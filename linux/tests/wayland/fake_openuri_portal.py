#!/usr/bin/env python3
"""A stand-in for org.freedesktop.portal.OpenURI in the nested test session:
it owns org.freedesktop.portal.Desktop and writes each OpenFile call (the
path of the file descriptor and the options) as a JSON line to the file
given as the first argument, instead of opening an application.
"""

import json
import os
import sys

import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

BUS_NAME = "org.freedesktop.portal.Desktop"
OBJECT_PATH = "/org/freedesktop/portal/desktop"
IFACE = "org.freedesktop.portal.OpenURI"


class OpenURI(dbus.service.Object):

  def __init__(self, bus, record):
    super().__init__(bus, OBJECT_PATH)
    self._record = record

  def _write(self, entry):
    with open(self._record, "a") as f:
      f.write(json.dumps(entry) + "\n")

  @dbus.service.method(IFACE, in_signature="sha{sv}", out_signature="o")
  def OpenFile(self, parent_window, fd, options):
    n = fd.take()
    try:
      path = os.readlink("/proc/self/fd/%d" % n)
    finally:
      os.close(n)
    self._write({"method": "OpenFile", "path": path,
                 "options": {str(k): bool(v) if isinstance(v, dbus.Boolean) else str(v)
                             for k, v in options.items()}})
    return dbus.ObjectPath("/org/freedesktop/portal/desktop/request/1_1/t")

  @dbus.service.method(IFACE, in_signature="ssa{sv}", out_signature="o")
  def OpenURI(self, parent_window, uri, options):
    self._write({"method": "OpenURI", "uri": str(uri)})
    return dbus.ObjectPath("/org/freedesktop/portal/desktop/request/1_1/t")


def main():
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SessionBus()
  name = dbus.service.BusName(BUS_NAME, bus)  # noqa: F841 (keeps the name owned)
  OpenURI(bus, sys.argv[1])
  print("ready", flush=True)
  GLib.MainLoop().run()


if __name__ == "__main__":
  main()
