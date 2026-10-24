#!/usr/bin/env python3
"""Isolated nested Wayland session for GUI tests.

Starts a private D-Bus daemon and a headless KWin (kwin_wayland --virtual)
with their own XDG_RUNTIME_DIR, HOME and XDG_*_HOME directories, runs test
programs inside, takes screenshots with Spectacle, and kills every process
of the session at the end. The user's real session, configuration and files
are never touched.

Python API:
    with Session(artifacts="out") as s:
        p = s.spawn(["./app"])
        s.screenshot("shot.png")

Command line:
    wlsession.py [--width W] [--height H] [--xwayland] [--keep] [--artifacts DIR] -- CMD [ARGS...]
runs CMD inside the session (with WAYLAND_DISPLAY, DBUS_SESSION_BUS_ADDRESS
and the isolated directories set) and exits with its status.
"""

import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

SOCKET_NAME = "wayland-z7test"
MARKER_VAR = "Z7_WLSESSION_ID"

# Variables of the outer session that must not leak into the nested one.
_DROP_VARS = (
    "DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET", "DBUS_SESSION_BUS_ADDRESS",
    "SESSION_MANAGER", "XDG_SESSION_ID", "XDG_SESSION_TYPE", "XDG_SESSION_CLASS",
    "XDG_SEAT", "XDG_VTNR", "XDG_ACTIVATION_TOKEN", "DESKTOP_STARTUP_ID",
    "KDE_SESSION_UID", "XAUTHORITY", "QT_QPA_PLATFORMTHEME",
    "MANGOHUD", "MANGOHUD_DLSYM", "LD_PRELOAD",
)


class SessionError(Exception):
  pass


def _js_string(s):
  return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def wait_until(predicate, timeout, what, interval=0.05):
  deadline = time.monotonic() + timeout
  while time.monotonic() < deadline:
    if predicate():
      return
    time.sleep(interval)
  raise SessionError("timed out waiting for " + what)


class Session:
  def __init__(self, width=1280, height=800, xwayland=False, artifacts=None, keep=False,
               dbus_services=()):
    self.width = width
    self.height = height
    self.xwayland = xwayland
    self.keep = keep
    self.dbus_services = list(dbus_services)
    self.id = uuid.uuid4().hex
    self.root = None
    self.env = None
    self.artifacts = artifacts
    self._procs = []
    self._logs = []

  # ---- lifecycle ----

  def __enter__(self):
    try:
      self.start()
    except BaseException:
      self.stop()
      raise
    return self

  def __exit__(self, *exc):
    self.stop()
    return False

  def start(self):
    self.root = tempfile.mkdtemp(prefix="z7-wlsession-")
    runtime = self._mkdir("runtime", 0o700)
    home = self._mkdir("home")
    if self.artifacts is None:
      self.artifacts = self._mkdir("artifacts")
    os.makedirs(self.artifacts, exist_ok=True)

    env = {k: v for k, v in os.environ.items() if k not in _DROP_VARS}
    env.update({
        MARKER_VAR: self.id,
        "XDG_RUNTIME_DIR": runtime,
        "HOME": home,
        "XDG_CONFIG_HOME": self._mkdir("home/.config"),
        "XDG_DATA_HOME": self._mkdir("home/.local/share"),
        "XDG_CACHE_HOME": self._mkdir("home/.cache"),
        "XDG_STATE_HOME": self._mkdir("home/.local/state"),
        "XDG_CURRENT_DESKTOP": "KDE",
        "KDE_FULL_SESSION": "true",
        "WAYLAND_DISPLAY": SOCKET_NAME,
        "QT_QPA_PLATFORM": "wayland",
        "QT_FORCE_STDERR_LOGGING": "1",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=%s/bus" % runtime,
    })
    self.env = env

    self.spawn(["dbus-daemon", "--config-file=" + self._write_dbus_config(), "--nofork",
                "--nopidfile"], log="dbus")
    wait_until(lambda: os.path.exists(os.path.join(runtime, "bus")), 10, "D-Bus socket")

    kwin = ["kwin_wayland", "--virtual", "--socket", SOCKET_NAME,
            "--width", str(self.width), "--height", str(self.height)]
    if self.xwayland:
      kwin.append("--xwayland")
    self.kwin = self.spawn(kwin, log="kwin")
    wait_until(lambda: os.path.exists(os.path.join(runtime, SOCKET_NAME)), 20, "KWin socket")
    wait_until(lambda: self._name_has_owner("org.kde.KWin"), 20, "org.kde.KWin on D-Bus")

  def stop(self):
    for p in reversed(self._procs):
      if p.poll() is None:
        p.terminate()
    deadline = time.monotonic() + 5
    for p in reversed(self._procs):
      try:
        p.wait(max(0.1, deadline - time.monotonic()))
      except subprocess.TimeoutExpired:
        pass
    # Anything started by the session (D-Bus activation, KIO workers, ...)
    # carries the marker variable, even if it detached from our process group.
    self._kill_marked(signal.SIGTERM)
    try:
      wait_until(lambda: not self._marked_pids(), 3, "session processes to exit")
    except SessionError:
      self._kill_marked(signal.SIGKILL)
    for f in self._logs:
      f.close()
    self._logs = []
    if self.root and not self.keep:
      shutil.rmtree(self.root, ignore_errors=True)

  # ---- running programs ----

  def spawn(self, argv, log=None, env=None, **kwargs):
    """Starts a program inside the session; output goes to <artifacts>/<log>.log."""
    full_env = dict(self.env)
    if env:
      full_env.update(env)
    name = log or os.path.basename(argv[0])
    if "stdout" not in kwargs:
      f = open(os.path.join(self.artifacts, name + ".log"), "w")
      self._logs.append(f)
      kwargs["stdout"] = f
      kwargs.setdefault("stderr", subprocess.STDOUT)
    p = subprocess.Popen(argv, env=full_env, start_new_session=True, **kwargs)
    self._procs.append(p)
    return p

  def run(self, argv, timeout=60, env=None, check=True):
    full_env = dict(self.env)
    if env:
      full_env.update(env)
    res = subprocess.run(argv, env=full_env, capture_output=True, text=True, timeout=timeout)
    if check and res.returncode != 0:
      raise SessionError("%s failed (%d): %s%s" % (argv[0], res.returncode, res.stdout[-2000:], res.stderr[-2000:]))
    return res

  def screenshot(self, path, timeout=30):
    """Captures the whole virtual screen (including window decorations)."""
    path = os.path.abspath(path)
    if os.path.exists(path):
      os.remove(path)
    self.run(["spectacle", "--background", "--nonotify", "--fullscreen", "--output", path],
             timeout=timeout)
    if not os.path.exists(path):
      raise SessionError("spectacle did not write " + path)
    return path

  def kwin_script(self, source, timeout=10):
    """Runs a KWin JavaScript snippet (for example to place windows)."""
    path = os.path.join(self.root, "kwin-script-%s.js" % uuid.uuid4().hex)
    with open(path, "w", encoding="utf-8") as f:
      f.write(source)
    plugin = "z7test_" + uuid.uuid4().hex
    res = self.run(["dbus-send", "--session", "--print-reply", "--dest=org.kde.KWin",
                    "/Scripting", "org.kde.kwin.Scripting.loadScript",
                    "string:" + path, "string:" + plugin], timeout=timeout)
    script_id = res.stdout.strip().split()[-1]
    self.run(["dbus-send", "--session", "--print-reply", "--dest=org.kde.KWin",
              "/Scripting/Script" + script_id, "org.kde.kwin.Script.run"], timeout=timeout)
    self.run(["dbus-send", "--session", "--print-reply", "--dest=org.kde.KWin",
              "/Scripting", "org.kde.kwin.Scripting.unloadScript", "string:" + plugin],
             timeout=timeout, check=False)

  def place_window(self, caption, x, y, width, height):
    """Moves and resizes the window whose caption is exactly CAPTION."""
    self.kwin_script("""
for (const w of workspace.windowList()) {
  if (w.caption === %s) {
    w.frameGeometry = { x: %d, y: %d, width: %d, height: %d };
    workspace.activeWindow = w;
  }
}
""" % (_js_string(caption), x, y, width, height))

  # ---- helpers ----

  def _mkdir(self, rel, mode=0o755):
    path = os.path.join(self.root, rel)
    os.makedirs(path, exist_ok=True)
    os.chmod(path, mode)
    return path

  def _write_dbus_config(self):
    # Only services listed in self.dbus_services, plus our FileTransfer portal
    # stand-in, can be D-Bus activated. The standard service directories are
    # left out on purpose: the real xdg-document-portal cannot mount FUSE in
    # the sandbox, and Qt then delivers empty drag-and-drop data.
    services = self._mkdir("dbus-services")
    for name in self.dbus_services:
      src = os.path.join("/usr/share/dbus-1/services", name + ".service")
      if not os.path.exists(src):
        raise SessionError("no D-Bus service file for " + name)
      shutil.copy(src, services)
    portal = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_filetransfer_portal.py")
    with open(os.path.join(services, "org.freedesktop.portal.Documents.service"), "w") as f:
      f.write("[D-BUS Service]\nName=org.freedesktop.portal.Documents\nExec=%s -I %s\n"
              % (sys.executable, portal))
    path = os.path.join(self.root, "dbus-session.conf")
    with open(path, "w", encoding="utf-8") as f:
      f.write("""<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <keep_umask/>
  <listen>%s</listen>
  <servicedir>%s</servicedir>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
""" % (self.env["DBUS_SESSION_BUS_ADDRESS"], services))
    return path

  def _name_has_owner(self, name):
    res = subprocess.run(
        ["dbus-send", "--session", "--print-reply", "--dest=org.freedesktop.DBus",
         "/org/freedesktop/DBus", "org.freedesktop.DBus.NameHasOwner", "string:" + name],
        env=self.env, capture_output=True, text=True)
    return res.returncode == 0 and "boolean true" in res.stdout

  def _marked_pids(self):
    needle = ("%s=%s" % (MARKER_VAR, self.id)).encode()
    pids = []
    for entry in os.listdir("/proc"):
      if not entry.isdigit() or int(entry) == os.getpid():
        continue
      try:
        with open("/proc/%s/environ" % entry, "rb") as f:
          if needle in f.read().split(b"\0"):
            pids.append(int(entry))
      except OSError:
        pass
    return pids

  def _kill_marked(self, sig):
    for pid in self._marked_pids():
      try:
        os.kill(pid, sig)
      except ProcessLookupError:
        pass


def main():
  ap = argparse.ArgumentParser(description="Run a command in an isolated nested KWin session.")
  ap.add_argument("--width", type=int, default=1280)
  ap.add_argument("--height", type=int, default=800)
  ap.add_argument("--xwayland", action="store_true")
  ap.add_argument("--keep", action="store_true", help="keep the session directory")
  ap.add_argument("--artifacts", help="directory for logs")
  ap.add_argument("cmd", nargs=argparse.REMAINDER)
  args = ap.parse_args()
  cmd = args.cmd[1:] if args.cmd[:1] == ["--"] else args.cmd
  if not cmd:
    ap.error("missing command")
  with Session(args.width, args.height, args.xwayland, args.artifacts, args.keep) as s:
    res = subprocess.run(cmd, env=s.env)
    return res.returncode


if __name__ == "__main__":
  sys.exit(main())
