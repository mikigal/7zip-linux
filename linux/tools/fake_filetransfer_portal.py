#!/usr/bin/env python3
"""Minimal org.freedesktop.portal.FileTransfer service for the nested test session.

Qt 6 routes file drag and drop and clipboard transfers through the document
portal's FileTransfer interface. The real xdg-document-portal needs a FUSE
mount, which is not available in the test sandbox, so wlsession.py
D-Bus-activates this stand-in instead. Like the real portal does for
unsandboxed receivers, it returns the original file paths.
"""

import os
import uuid

import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

BUS_NAME = "org.freedesktop.portal.Documents"
OBJECT_PATH = "/org/freedesktop/portal/documents"
IFACE = "org.freedesktop.portal.FileTransfer"
VERSION = 1


class FileTransfer(dbus.service.Object):

  def __init__(self, bus):
    super().__init__(bus, OBJECT_PATH)
    self._transfers = {}  # key -> {"files": [...], "autostop": bool}

  @dbus.service.method(IFACE, in_signature="a{sv}", out_signature="s")
  def StartTransfer(self, options):
    key = uuid.uuid4().hex
    self._transfers[key] = {"files": [], "autostop": bool(options.get("autostop", True))}
    return key

  @dbus.service.method(IFACE, in_signature="saha{sv}", out_signature="")
  def AddFiles(self, key, fds, options):
    transfer = self._get(key)
    for fd in fds:
      n = fd.take()
      try:
        transfer["files"].append(os.readlink("/proc/self/fd/%d" % n))
      finally:
        os.close(n)

  @dbus.service.method(IFACE, in_signature="sa{sv}", out_signature="as")
  def RetrieveFiles(self, key, options):
    transfer = self._get(key)
    files = list(transfer["files"])
    if transfer["autostop"]:
      self._stop(key)
    return files

  @dbus.service.method(IFACE, in_signature="s", out_signature="")
  def StopTransfer(self, key):
    self._get(key)
    self._stop(key)

  @dbus.service.signal(IFACE, signature="s")
  def TransferClosed(self, key):
    pass

  @dbus.service.method(dbus.PROPERTIES_IFACE, in_signature="ss", out_signature="v")
  def Get(self, interface, prop):
    if interface == IFACE and prop == "version":
      return dbus.UInt32(VERSION)
    raise dbus.exceptions.DBusException("org.freedesktop.DBus.Error.UnknownProperty", prop)

  @dbus.service.method(dbus.PROPERTIES_IFACE, in_signature="s", out_signature="a{sv}")
  def GetAll(self, interface):
    return {"version": dbus.UInt32(VERSION)} if interface == IFACE else {}

  def _get(self, key):
    if key not in self._transfers:
      raise dbus.exceptions.DBusException("org.freedesktop.portal.Error.NotFound",
                                          "Invalid transfer key")
    return self._transfers[key]

  def _stop(self, key):
    del self._transfers[key]
    self.TransferClosed(key)


def main():
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SessionBus()
  name = dbus.service.BusName(BUS_NAME, bus)  # noqa: F841 (keeps the name owned)
  FileTransfer(bus)
  GLib.MainLoop().run()


if __name__ == "__main__":
  main()
