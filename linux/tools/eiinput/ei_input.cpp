// ei_input.cpp
// Injects pointer and keyboard input into a KWin Wayland session through
// libei. The EIS connection is obtained from KWin over D-Bus
// (org.kde.KWin.EIS.RemoteDesktop.connectToEIS), so this works in the nested
// headless KWin started by linux/tools/wlsession.py.
//
// Usage: ei-input COMMAND [COMMAND...]
//   move X Y                   absolute pointer motion in logical pixels
//   press [BUTTON]             BUTTON: left (default), right, middle
//   release [BUTTON]
//   click [BUTTON]
//   drag X1 Y1 X2 Y2 [STEPS]   press at (X1,Y1), move in STEPS, release at (X2,Y2)
//   key NAME[+NAME...]         press and release a combination, e.g. ctrl+a, f5
//   keydown NAME | keyup NAME
//   type TEXT                  ASCII text, US layout
//   wait MS

#include <libei.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QElapsedTimer>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

[[noreturn]] void Fail(const std::string &msg)
{
  std::fprintf(stderr, "ei-input: %s\n", msg.c_str());
  std::exit(1);
}

// Capability bits of org.kde.KWin.EIS.RemoteDesktop.connectToEIS
// (same values as the xdg-desktop-portal RemoteDesktop device types).
const int kCapKeyboard = 1;
const int kCapPointer = 2;

struct CState
{
  struct ei *Ei = nullptr;
  struct ei_device *Abs = nullptr;
  struct ei_device *Buttons = nullptr;
  struct ei_device *Keyboard = nullptr;
  uint32_t Sequence = 0;
  bool Disconnected = false;
};

bool HasAll(struct ei_device *d, std::initializer_list<ei_device_capability> caps)
{
  for (auto c : caps)
    if (!ei_device_has_capability(d, c))
      return false;
  return true;
}

void HandleEvents(CState &st)
{
  ei_dispatch(st.Ei);
  struct ei_event *e;
  while ((e = ei_get_event(st.Ei)) != nullptr)
  {
    switch (ei_event_get_type(e))
    {
      case EI_EVENT_SEAT_ADDED:
        ei_seat_bind_capabilities(ei_event_get_seat(e),
            EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE,
            EI_DEVICE_CAP_BUTTON, EI_DEVICE_CAP_SCROLL,
            EI_DEVICE_CAP_KEYBOARD, nullptr);
        break;
      case EI_EVENT_DEVICE_RESUMED:
      {
        struct ei_device *d = ei_event_get_device(e);
        if (!st.Abs && HasAll(d, {EI_DEVICE_CAP_POINTER_ABSOLUTE}))
          st.Abs = ei_device_ref(d);
        if (!st.Buttons && HasAll(d, {EI_DEVICE_CAP_BUTTON}))
          st.Buttons = ei_device_ref(d);
        if (!st.Keyboard && HasAll(d, {EI_DEVICE_CAP_KEYBOARD}))
          st.Keyboard = ei_device_ref(d);
        ei_device_start_emulating(d, ++st.Sequence);
        break;
      }
      case EI_EVENT_DISCONNECT:
        st.Disconnected = true;
        break;
      default:
        break;
    }
    ei_event_unref(e);
  }
}

// Processes incoming events for the given time (keeps the connection alive
// and gives the compositor time to deliver what we sent).
void Pump(CState &st, int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    struct pollfd pfd = { ei_get_fd(st.Ei), POLLIN, 0 };
    const int left = ms - (int)t.elapsed();
    poll(&pfd, 1, left > 0 ? left : 0);
    HandleEvents(st);
    if (st.Disconnected)
      Fail("disconnected by the compositor");
  }
  while (t.elapsed() < ms);
}

const std::map<std::string, uint32_t> &KeyNames()
{
  static const std::map<std::string, uint32_t> names = {
    {"esc", KEY_ESC}, {"enter", KEY_ENTER}, {"return", KEY_ENTER}, {"tab", KEY_TAB},
    {"space", KEY_SPACE}, {"backspace", KEY_BACKSPACE}, {"delete", KEY_DELETE},
    {"insert", KEY_INSERT}, {"home", KEY_HOME}, {"end", KEY_END},
    {"pageup", KEY_PAGEUP}, {"pagedown", KEY_PAGEDOWN},
    {"up", KEY_UP}, {"down", KEY_DOWN}, {"left", KEY_LEFT}, {"right", KEY_RIGHT},
    {"ctrl", KEY_LEFTCTRL}, {"shift", KEY_LEFTSHIFT}, {"alt", KEY_LEFTALT},
    {"meta", KEY_LEFTMETA}, {"menu", KEY_COMPOSE},
    {"minus", KEY_MINUS}, {"equal", KEY_EQUAL}, {"plus", KEY_KPPLUS},
    {"kpminus", KEY_KPMINUS}, {"kpasterisk", KEY_KPASTERISK},
    {"f1", KEY_F1}, {"f2", KEY_F2}, {"f3", KEY_F3}, {"f4", KEY_F4}, {"f5", KEY_F5},
    {"f6", KEY_F6}, {"f7", KEY_F7}, {"f8", KEY_F8}, {"f9", KEY_F9}, {"f10", KEY_F10},
    {"f11", KEY_F11}, {"f12", KEY_F12},
  };
  return names;
}

const uint32_t kLetterKeys[26] = {
  KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
  KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
};
const uint32_t kDigitKeys[10] = {
  KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9
};

uint32_t KeyCode(const std::string &name)
{
  if (name.size() == 1)
  {
    const char c = name[0];
    if (c >= 'a' && c <= 'z') return kLetterKeys[c - 'a'];
    if (c >= '0' && c <= '9') return kDigitKeys[c - '0'];
  }
  auto it = KeyNames().find(name);
  if (it == KeyNames().end())
    Fail("unknown key name: " + name);
  return it->second;
}

// Maps an ASCII character to (keycode, needs shift) for a US layout.
bool CharKey(char c, uint32_t &code, bool &shift)
{
  static const char *kShifted = ")!@#$%^&*(";
  shift = false;
  if (c >= 'a' && c <= 'z') { code = kLetterKeys[c - 'a']; return true; }
  if (c >= 'A' && c <= 'Z') { code = kLetterKeys[c - 'A']; shift = true; return true; }
  if (c >= '0' && c <= '9') { code = kDigitKeys[c - '0']; return true; }
  if (const char *p = std::strchr(kShifted, c)) { code = kDigitKeys[p - kShifted]; shift = true; return true; }
  static const std::map<char, std::pair<uint32_t, bool>> others = {
    {' ', {KEY_SPACE, false}}, {'-', {KEY_MINUS, false}}, {'_', {KEY_MINUS, true}},
    {'=', {KEY_EQUAL, false}}, {'+', {KEY_EQUAL, true}}, {'.', {KEY_DOT, false}},
    {',', {KEY_COMMA, false}}, {'/', {KEY_SLASH, false}}, {'?', {KEY_SLASH, true}},
    {';', {KEY_SEMICOLON, false}}, {':', {KEY_SEMICOLON, true}},
    {'\'', {KEY_APOSTROPHE, false}}, {'"', {KEY_APOSTROPHE, true}},
    {'\n', {KEY_ENTER, false}},
  };
  auto it = others.find(c);
  if (it == others.end())
    return false;
  code = it->second.first;
  shift = it->second.second;
  return true;
}

uint32_t ButtonCode(const std::string &name)
{
  if (name == "left") return BTN_LEFT;
  if (name == "right") return BTN_RIGHT;
  if (name == "middle") return BTN_MIDDLE;
  Fail("unknown button: " + name);
}

class CRunner
{
  CState &_st;
  std::vector<std::string> _args;
  size_t _pos = 0;

  bool HasArg() const { return _pos < _args.size(); }
  std::string Next(const char *what)
  {
    if (!HasArg())
      Fail(std::string("missing ") + what);
    return _args[_pos++];
  }
  double NextNum(const char *what) { return std::atof(Next(what).c_str()); }
  bool PeekIsButton() const
  {
    return HasArg() && (_args[_pos] == "left" || _args[_pos] == "right" || _args[_pos] == "middle");
  }

  struct ei_device *Need(struct ei_device *d, const char *what)
  {
    if (!d)
      Fail(std::string("compositor provided no ") + what + " device");
    return d;
  }

  void Frame(struct ei_device *d, int pumpMs = 15)
  {
    ei_device_frame(d, ei_now(_st.Ei));
    Pump(_st, pumpMs);
  }

  void Move(double x, double y)
  {
    struct ei_device *d = Need(_st.Abs, "absolute pointer");
    if (!ei_device_get_region_at(d, x, y))
      std::fprintf(stderr, "ei-input: warning: (%g, %g) is outside all device regions\n", x, y);
    ei_device_pointer_motion_absolute(d, x, y);
    Frame(d);
  }

  void Button(uint32_t code, bool press)
  {
    struct ei_device *d = Need(_st.Buttons, "button");
    ei_device_button_button(d, code, press);
    Frame(d, 30);
  }

  void Key(uint32_t code, bool press)
  {
    struct ei_device *d = Need(_st.Keyboard, "keyboard");
    ei_device_keyboard_key(d, code, press);
    Frame(d);
  }

public:
  CRunner(CState &st, std::vector<std::string> args): _st(st), _args(std::move(args)) {}

  void Run()
  {
    while (HasArg())
    {
      const std::string cmd = Next("command");
      if (cmd == "move")
      {
        const double x = NextNum("x");
        Move(x, NextNum("y"));
      }
      else if (cmd == "press" || cmd == "release" || cmd == "click")
      {
        const uint32_t b = ButtonCode(PeekIsButton() ? Next("button") : "left");
        if (cmd != "release") Button(b, true);
        if (cmd != "press") Button(b, false);
      }
      else if (cmd == "drag")
      {
        const double x1 = NextNum("x1"), y1 = NextNum("y1");
        const double x2 = NextNum("x2"), y2 = NextNum("y2");
        int steps = 20;
        if (HasArg() && _args[_pos].find_first_not_of("0123456789") == std::string::npos)
          steps = std::atoi(Next("steps").c_str());
        Move(x1, y1);
        Button(BTN_LEFT, true);
        Pump(_st, 100);
        for (int i = 1; i <= steps; i++)
        {
          Move(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps);
          Pump(_st, 15);
        }
        Pump(_st, 200);
        Button(BTN_LEFT, false);
      }
      else if (cmd == "key" || cmd == "keydown" || cmd == "keyup")
      {
        std::vector<uint32_t> codes;
        std::string combo = Next("key name");
        size_t start = 0;
        for (;;)
        {
          const size_t plus = combo.find('+', start);
          codes.push_back(KeyCode(combo.substr(start, plus == std::string::npos ? plus : plus - start)));
          if (plus == std::string::npos)
            break;
          start = plus + 1;
        }
        if (cmd != "keyup")
          for (uint32_t c : codes) Key(c, true);
        if (cmd != "keydown")
          for (auto it = codes.rbegin(); it != codes.rend(); ++it) Key(*it, false);
      }
      else if (cmd == "type")
      {
        for (char c : Next("text"))
        {
          uint32_t code;
          bool shift;
          if (!CharKey(c, code, shift))
            Fail(std::string("cannot type character '") + c + "'");
          if (shift) Key(KEY_LEFTSHIFT, true);
          Key(code, true);
          Key(code, false);
          if (shift) Key(KEY_LEFTSHIFT, false);
        }
      }
      else if (cmd == "wait")
        Pump(_st, std::atoi(Next("milliseconds").c_str()));
      else
        Fail("unknown command: " + cmd);
    }
  }
};

}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty() || args[0] == "-h" || args[0] == "--help")
  {
    std::fprintf(stderr, "usage: ei-input COMMAND... (see the comment at the top of ei_input.cpp)\n");
    return args.empty() ? 2 : 0;
  }

  QDBusMessage call = QDBusMessage::createMethodCall(
      "org.kde.KWin", "/org/kde/KWin/EIS/RemoteDesktop",
      "org.kde.KWin.EIS.RemoteDesktop", "connectToEIS");
  call << (kCapKeyboard | kCapPointer);
  const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 5000);
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2)
    Fail("connectToEIS failed: " + reply.errorMessage().toStdString());
  const int fd = dup(reply.arguments().at(0).value<QDBusUnixFileDescriptor>().fileDescriptor());
  const int cookie = reply.arguments().at(1).toInt();
  if (fd < 0)
    Fail("connectToEIS returned no file descriptor");

  CState st;
  st.Ei = ei_new_sender(nullptr);
  ei_configure_name(st.Ei, "z7-ei-input");
  if (ei_setup_backend_fd(st.Ei, fd) != 0)
    Fail("ei_setup_backend_fd failed");

  // Wait until the compositor has given us the devices we need.
  QElapsedTimer t;
  t.start();
  while (!(st.Abs && st.Buttons && st.Keyboard) && t.elapsed() < 5000)
    Pump(st, 20);
  if (!st.Abs && !st.Buttons && !st.Keyboard)
    Fail("no input devices were provided by the compositor");

  CRunner(st, args).Run();

  Pump(st, 50);
  for (struct ei_device *d : { st.Abs, st.Buttons, st.Keyboard })
    if (d)
      ei_device_stop_emulating(d);
  Pump(st, 20);
  for (struct ei_device *d : { st.Abs, st.Buttons, st.Keyboard })
    if (d)
      ei_device_unref(d);
  ei_unref(st.Ei);

  QDBusMessage disconnect = QDBusMessage::createMethodCall(
      "org.kde.KWin", "/org/kde/KWin/EIS/RemoteDesktop",
      "org.kde.KWin.EIS.RemoteDesktop", "disconnect");
  disconnect << cookie;
  QDBusConnection::sessionBus().call(disconnect, QDBus::Block, 2000);
  return 0;
}
