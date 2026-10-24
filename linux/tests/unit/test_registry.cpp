// test_registry.cpp
// Unit tests for the INI-backed NWindows::NRegistry::CKey
// (linux/platform/Registry.cpp) and for the upstream settings code that uses
// it unchanged (UI/Common/ZipRegistry, FileManager/RegistryUtils and
// FileManager/ViewSettings).
//
// The binary also has child modes used by the multi-process tests:
//   --print-path               print the default INI path and exit
//   --writer INI ID COUNT      write COUNT values, one CKey session each

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "Win32Compat.h"

#include "../../../CPP/Common/UTFConvert.h"
#include "../../../CPP/Windows/Registry.h"
#include "../../../CPP/7zip/PropID.h"
#include "../../../CPP/7zip/UI/Common/ZipRegistry.h"
#include "../../../CPP/7zip/UI/FileManager/RegistryUtils.h"
#include "../../../CPP/7zip/UI/FileManager/ViewSettings.h"

#include "RegistryIni.h"

using namespace NWindows;
using namespace NRegistry;

static const char * const kTestKey = "Software/7-Zip/Test";

static FString ToFString(const QString &s)
{
  return FString(s.toUtf8().constData());
}

static QString ToQString(const UString &s)
{
  return QString::fromWCharArray(s.Ptr(), (int)s.Len());
}

static QByteArray ReadFile(const QString &path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return QByteArray();
  return f.readAll();
}

static void WriteFile(const QString &path, const QByteArray &data)
{
  // a new inode, as a real writer (temporary file plus rename) produces
  const QString temp = path + ".test-tmp";
  QFile f(temp);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(f.write(data), data.size());
  f.close();
  QVERIFY(::rename(temp.toUtf8().constData(), path.toUtf8().constData()) == 0);
}

class CRegistryTest: public QObject
{
  Q_OBJECT

  QTemporaryDir _dir;
  QString _ini;
  int _counter = 0;

  // drops the in-memory state, as a new process would
  void Reopen() { Ini_SetFilePath(ToFString(_ini)); }

private slots:
  void init()
  {
    QVERIFY(_dir.isValid());
    _ini = _dir.filePath(QString("cfg%1/7-Zip/7-Zip.ini").arg(++_counter));
    Reopen();
  }

  void valueTypes()
  {
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, kTestKey), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue("u32", (UInt32)0xdeadbeef), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue("bool", true), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue("narrow", "abc"), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue(L"wide", L"za\u017c\u00f3\u0142\u0107 \U0001F600"), (LONG)ERROR_SUCCESS);
      const Byte bin[] = { 0, 1, 2, 0xff, 0 };
      QCOMPARE(key.SetValue("bin", bin, sizeof(bin)), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue("empty-bin", bin, 0), (LONG)ERROR_SUCCESS);
      UStringVector strings;
      strings.Add(L"/home/user");
      strings.Add(L"");
      strings.Add(L"C:\\x \"y\"");
      QCOMPARE(key.SetValue_Strings("multi", strings), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue_Strings("empty-multi", UStringVector()), (LONG)ERROR_SUCCESS);
    }
    QVERIFY(QFile::exists(_ini));

    for (int pass = 0; pass < 2; pass++)
    {
      if (pass == 1)
        Reopen();
      CKey key;
      QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
      UInt32 u = 0;
      QCOMPARE(key.GetValue_UInt32_IfOk("u32", u), (LONG)ERROR_SUCCESS);
      QCOMPARE(u, (UInt32)0xdeadbeef);
      bool b = false;
      QCOMPARE(key.GetValue_bool_IfOk("bool", b), (LONG)ERROR_SUCCESS);
      QVERIFY(b);
      CSysString narrow;
      QCOMPARE(key.QueryValue("narrow", narrow), (LONG)ERROR_SUCCESS);
      QCOMPARE(QString(narrow.Ptr()), QString("abc"));
      UString wide;
      QCOMPARE(key.QueryValue(L"wide", wide), (LONG)ERROR_SUCCESS);
      QCOMPARE(ToQString(wide), QString::fromUtf8("za\u017c\u00f3\u0142\u0107 \U0001F600"));
      CByteBuffer bin;
      QCOMPARE(key.QueryValue_Binary("bin", bin), (LONG)ERROR_SUCCESS);
      QCOMPARE(QByteArray((const char *)(const Byte *)bin, (int)bin.Size()), QByteArray("\0\1\2\xff\0", 5));
      QCOMPARE(key.QueryValue_Binary("empty-bin", bin), (LONG)ERROR_SUCCESS);
      QCOMPARE((int)bin.Size(), 0);
      UStringVector strings;
      QCOMPARE(key.GetValue_Strings("multi", strings), (LONG)ERROR_SUCCESS);
      QCOMPARE((int)strings.Size(), 3);
      QCOMPARE(ToQString(strings[0]), QString("/home/user"));
      QCOMPARE(ToQString(strings[1]), QString());
      QCOMPARE(ToQString(strings[2]), QString("C:\\x \"y\""));
      QCOMPARE(key.GetValue_Strings("empty-multi", strings), (LONG)ERROR_SUCCESS);
      QCOMPARE((int)strings.Size(), 0);

      // type mismatches and missing values leave the output unchanged
      u = 7;
      QCOMPARE(key.GetValue_UInt32_IfOk("narrow", u), (LONG)ERROR_UNSUPPORTED_TYPE);
      QCOMPARE(u, (UInt32)7);
      QVERIFY(key.GetValue_UInt32_IfOk("missing", u) != ERROR_SUCCESS);
      QCOMPARE(u, (UInt32)7);
      QCOMPARE(key.QueryValue("u32", narrow), (LONG)ERROR_UNSUPPORTED_TYPE);
      QVERIFY(narrow.IsEmpty());
      QCOMPARE(key.QueryValue_Binary("narrow", bin), (LONG)ERROR_UNSUPPORTED_TYPE);
      UInt64 u64;
      QCOMPARE(key.GetValue_UInt64_IfOk("u32", u64), (LONG)ERROR_UNSUPPORTED_TYPE);
    }
  }

  void stringEscapes_data()
  {
    QTest::addColumn<QByteArray>("value");
    QTest::newRow("quotes and backslashes") << QByteArray("a\"b\\c\\\"");
    QTest::newRow("control characters") << QByteArray("line1\nline2\r\t\x01\x1f\x7f");
    QTest::newRow("brackets and equals") << QByteArray("[x]=y ; z # w");
    QTest::newRow("leading and trailing spaces") << QByteArray("  padded  ");
    QTest::newRow("utf-8") << QByteArray("\xc5\xbc\xc3\xb3\xc5\x82w \xf0\x9f\x98\x80");
    QTest::newRow("invalid utf-8") << QByteArray("bad\xff\xfe \xc3( \xed\xa0\x80 \xf4\x90\x80\x80 end\xe2\x82");
    QTest::newRow("escape-like text") << QByteArray("\\x41\\n");
  }

  void stringEscapes()
  {
    QFETCH(QByteArray, value);
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, kTestKey), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue("v", value.constData()), (LONG)ERROR_SUCCESS);
      // the same bytes as a value name
      QCOMPARE(key.SetValue(value.constData(), (UInt32)1), (LONG)ERROR_SUCCESS);
    }
    const QByteArray file = ReadFile(_ini);
    QVERIFY(!QString::fromUtf8(file).contains(QChar::ReplacementCharacter)); // the file stays valid UTF-8
    QCOMPARE(file.count('\n'), 5); // comment, blank line, section, two values

    Reopen();
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
    CSysString s;
    QCOMPARE(key.QueryValue("v", s), (LONG)ERROR_SUCCESS);
    QCOMPARE(QByteArray(s.Ptr(), (int)s.Len()), value);
    UInt32 u = 0;
    QCOMPARE(key.GetValue_UInt32_IfOk(value.constData(), u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)1);
  }

  void unicodeRoundTrip()
  {
    // file names that are not valid UTF-8 reach UString as upstream escape
    // codes; they must come back as the same UString
    const AString raw ("name-\xff\xc3(-\xc4\x85");
    UString u;
    ConvertUTF8ToUnicode(raw, u);
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, kTestKey), (LONG)ERROR_SUCCESS);
      QCOMPARE(key.SetValue(L"w", u), (LONG)ERROR_SUCCESS);
      UStringVector v;
      v.Add(u);
      QCOMPARE(key.SetValue_Strings("m", v), (LONG)ERROR_SUCCESS);
    }
    Reopen();
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
    UString r;
    QCOMPARE(key.QueryValue(L"w", r), (LONG)ERROR_SUCCESS);
    QVERIFY(r == u);
    AString back;
    ConvertUnicodeToUTF8(r, back);
    QCOMPARE(QByteArray(back.Ptr()), QByteArray(raw.Ptr()));
    UStringVector v;
    QCOMPARE(key.GetValue_Strings("m", v), (LONG)ERROR_SUCCESS);
    QCOMPARE((int)v.Size(), 1);
    QVERIFY(v[0] == u);
  }

  void fileFormat()
  {
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, "Software/7-Zip/FM"), (LONG)ERROR_SUCCESS);
      key.SetValue("ShowDots", true);
      key.SetValue(L"Lang", L"pl");
      const Byte bin[] = { 1, 0, 0xab };
      key.SetValue("Columns", bin, sizeof(bin));
      UStringVector h;
      h.Add(L"/home/user");
      h.Add(L"/tmp");
      key.SetValue_Strings("PathHistory", h);
      key.SetValue(NULL, "default");
      CKey sub;
      QCOMPARE(sub.Create(key, "Empty"), (LONG)ERROR_SUCCESS);
    }
    const QString text = QString::fromUtf8(ReadFile(_ini));
    QCOMPARE(text,
        QString("; 7-Zip settings. 7-Zip rewrites this file; comments are not kept.\n"
        "\n"
        "[Software/7-Zip/FM]\n"
        "\"ShowDots\"=dword:00000001\n"
        "\"Lang\"=\"pl\"\n"
        "\"Columns\"=hex:01,00,ab\n"
        "\"PathHistory\"=multi:\"/home/user\",\"/tmp\"\n"
        "@=\"default\"\n"
        "\n"
        "[Software/7-Zip/FM/Empty]\n"));

    struct stat st;
    QCOMPARE(stat(_ini.toUtf8().constData(), &st), 0);
    QCOMPARE((int)(st.st_mode & 0777), 0600);
    QCOMPARE(stat(QFileInfo(_ini).path().toUtf8().constData(), &st), 0);
    QCOMPARE((int)(st.st_mode & 0777), 0700);
  }

  void parseHandWrittenFile()
  {
    QDir().mkpath(QFileInfo(_ini).path());
    WriteFile(_ini,
        "; comment\r\n"
        "# another comment\n"
        "garbage line\n"
        "\"orphan\"=dword:1\n"
        "  [ Software/7-Zip/Test ]  \n"
        "\"a\"=dword:2a\n"
        "\"b\" = \"text\"\r\n"
        "\"c\"=hex: 01 , 02,03\n"
        "\"d\"=qword:100000000\n"
        "\"e\"=hex(2):41,00\n"
        "\"f\"=expand:\"%HOME%\"\n"
        "\"bad1\"=dword:123456789\n"
        "\"bad2\"=\"unterminated\n"
        "\"bad3\"=hex:0g\n"
        "\"bad4\"=\"zero\\x00byte\"\n"
        "\"bad5\"=multi:\"a\" \"b\"\n"
        "\"A\"=dword:2b\n"
        "[Software//7-Zip\\]/Other]\n"
        "@=\"x\"\n"
        "[broken\n"
        "\"lost\"=dword:1\n");
    Reopen();
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
    UInt32 u = 0;
    QCOMPARE(key.GetValue_UInt32_IfOk("a", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)0x2b); // the later "A" replaces "a"
    CSysString s;
    QCOMPARE(key.QueryValue("b", s), (LONG)ERROR_SUCCESS);
    QCOMPARE(QString(s.Ptr()), QString("text"));
    CByteBuffer bin;
    QCOMPARE(key.QueryValue_Binary("c", bin), (LONG)ERROR_SUCCESS);
    QCOMPARE((int)bin.Size(), 3);
    UInt64 u64 = 0;
    QCOMPARE(key.GetValue_UInt64_IfOk("d", u64), (LONG)ERROR_SUCCESS);
    QCOMPARE(u64, (UInt64)1 << 32);
    QCOMPARE(key.QueryValue("e", s), (LONG)ERROR_SUCCESS); // REG_EXPAND_SZ
    QCOMPARE(QString(s.Ptr()), QString("A"));
    QCOMPARE(key.QueryValue("f", s), (LONG)ERROR_SUCCESS);
    QCOMPARE(QString(s.Ptr()), QString("%HOME%"));
    for (const char *bad : { "bad1", "bad2", "bad3", "bad4", "bad5", "orphan", "lost" })
      QVERIFY2(key.QueryValue(bad, s) != ERROR_SUCCESS && key.GetValue_UInt32_IfOk(bad, u) != ERROR_SUCCESS, bad);

    CKey other;
    QCOMPARE(other.Open(HKEY_CURRENT_USER, "Software/7-Zip]/Other", KEY_READ), (LONG)ERROR_SUCCESS);
    QCOMPARE(other.QueryValue(NULL, s), (LONG)ERROR_SUCCESS);
    QCOMPARE(QString(s.Ptr()), QString("x"));
  }

  void caseInsensitiveNames()
  {
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, "Software/7-Zip/CaseTest"), (LONG)ERROR_SUCCESS);
      key.SetValue("Value", (UInt32)1);
      key.SetValue("VALUE", (UInt32)2);
      key.SetValue(L"\u017B\u00D3\u0141W", L"x"); // ŻÓŁW
    }
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, "SOFTWARE/7-zip/casetest", KEY_READ), (LONG)ERROR_SUCCESS);
    UInt32 u = 0;
    QCOMPARE(key.GetValue_UInt32_IfOk("value", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)2);
    UString w;
    QCOMPARE(key.QueryValue(L"\u017C\u00F3\u0142w", w), (LONG)ERROR_SUCCESS); // żółw
    QVERIFY(!QString::fromUtf8(ReadFile(_ini)).contains("VALUE"));
  }

  void keys()
  {
    CKey root;
    DWORD disposition = 0;
    QCOMPARE(root.Create(HKEY_CURRENT_USER, "Software\\7-Zip/Keys", NULL,
        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &disposition), (LONG)ERROR_SUCCESS);
    QCOMPARE(disposition, (DWORD)REG_CREATED_NEW_KEY);
    {
      CKey again;
      QCOMPARE(again.Create(HKEY_CURRENT_USER, "Software/7-Zip/Keys", NULL,
          REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &disposition), (LONG)ERROR_SUCCESS);
      QCOMPARE(disposition, (DWORD)REG_OPENED_EXISTING_KEY);
    }
    for (const char *name : { "zip", "7z", "tar" })
    {
      CKey k;
      QCOMPARE(k.Create(root, name), (LONG)ERROR_SUCCESS);
    }
    {
      CKey deep;
      QCOMPARE(deep.Create(root, "tar/a/b"), (LONG)ERROR_SUCCESS);
      QCOMPARE(deep.SetValue("v", (UInt32)1), (LONG)ERROR_SUCCESS);
    }
    CSysStringVector names;
    QCOMPARE(root.EnumKeys(names), (LONG)ERROR_SUCCESS);
    QCOMPARE((int)names.Size(), 3);
    QCOMPARE(QString(names[0].Ptr()), QString("zip"));
    QCOMPARE(QString(names[1].Ptr()), QString("7z"));
    QCOMPARE(QString(names[2].Ptr()), QString("tar"));

    CKey missing;
    QVERIFY(missing.Open(root, "rar", KEY_READ) != ERROR_SUCCESS);
    QVERIFY(missing.Open(HKEY_CURRENT_USER, "Software/7-Zip/Keys/tar/a", KEY_READ) == ERROR_SUCCESS);
    missing.Close();

    QCOMPARE(root.DeleteSubKey("tar"), (LONG)ERROR_ACCESS_DENIED); // has subkeys
    QCOMPARE(root.DeleteSubKey("zip"), (LONG)ERROR_SUCCESS);
    QVERIFY(root.DeleteSubKey("zip") != ERROR_SUCCESS);
    QCOMPARE(root.RecurseDeleteKey("tar"), (LONG)ERROR_SUCCESS);
    QVERIFY(root.RecurseDeleteKey("tar") != ERROR_SUCCESS);
    QCOMPARE(root.EnumKeys(names), (LONG)ERROR_SUCCESS);
    QCOMPARE((int)names.Size(), 1);

    QCOMPARE(root.SetValue("x", (UInt32)1), (LONG)ERROR_SUCCESS);
    QCOMPARE(root.DeleteValue("x"), (LONG)ERROR_SUCCESS);
    QVERIFY(root.DeleteValue("x") != ERROR_SUCCESS);
    QCOMPARE(root.SetKeyValue("kv", "name", "value"), (LONG)ERROR_SUCCESS);
    QCOMPARE(NRegistry::SetValue(HKEY_CURRENT_USER, "Software/7-Zip/Keys/kv2", "name", "value"), (LONG)ERROR_SUCCESS);
    root.Close(); // the last open key: changes are written now

    // the empty key "7z" and the new keys persist
    Reopen();
    CKey k;
    QCOMPARE(k.Open(HKEY_CURRENT_USER, "Software/7-Zip/Keys", KEY_READ), (LONG)ERROR_SUCCESS);
    QCOMPARE(k.EnumKeys(names), (LONG)ERROR_SUCCESS);
    QCOMPARE((int)names.Size(), 3);
    QCOMPARE(QString(names[0].Ptr()), QString("7z"));
    CSysString s;
    CKey kv;
    QCOMPARE(kv.Open(k, "kv2", KEY_READ), (LONG)ERROR_SUCCESS);
    QCOMPARE(kv.QueryValue("name", s), (LONG)ERROR_SUCCESS);
    QCOMPARE(QString(s.Ptr()), QString("value"));
  }

  void otherRootsAreReadOnly()
  {
    CKey key;
    QVERIFY(key.Open(HKEY_LOCAL_MACHINE, "Software/7-Zip", KEY_READ) != ERROR_SUCCESS);
    QVERIFY(key.Create(HKEY_LOCAL_MACHINE, "Software/7-Zip") != ERROR_SUCCESS);
    QVERIFY(key.Create(HKEY_CLASSES_ROOT, ".7z") != ERROR_SUCCESS);
    QVERIFY(!QFile::exists(_ini));
  }

  void seesChangesOfOtherProcesses()
  {
    {
      CKey key;
      key.Create(HKEY_CURRENT_USER, kTestKey);
      key.SetValue("a", (UInt32)1);
    }
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
    WriteFile(_ini, "[Software/7-Zip/Test]\n\"a\"=dword:00000002\n");
    UInt32 u = 0;
    QCOMPARE(key.GetValue_UInt32_IfOk("a", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)2);
  }

  void mergesWithConcurrentWriter()
  {
    {
      CKey key;
      key.Create(HKEY_CURRENT_USER, kTestKey);
      key.SetValue("shared", (UInt32)1);
      key.SetValue("gone", (UInt32)5);
    }
    {
      CKey key;
      QCOMPARE(key.Create(HKEY_CURRENT_USER, kTestKey), (LONG)ERROR_SUCCESS);
      key.SetValue("mine", (UInt32)10);
      key.SetValue("shared", (UInt32)3);
      QCOMPARE(key.DeleteValue("gone"), (LONG)ERROR_SUCCESS);
      // another process writes while this key is still open
      WriteFile(_ini,
          "[Software/7-Zip/Test]\n\"shared\"=dword:00000002\n\"theirs\"=dword:00000014\n\"gone\"=dword:0\n"
          "[Software/7-Zip/Other]\n\"x\"=\"y\"\n");
      UInt32 u = 0;
      QCOMPARE(key.GetValue_UInt32_IfOk("theirs", u), (LONG)ERROR_SUCCESS);
      QCOMPARE(u, (UInt32)20);
      QCOMPARE(key.GetValue_UInt32_IfOk("shared", u), (LONG)ERROR_SUCCESS);
      QCOMPARE(u, (UInt32)3); // our pending change wins
    }
    Reopen();
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ), (LONG)ERROR_SUCCESS);
    UInt32 u = 0;
    QCOMPARE(key.GetValue_UInt32_IfOk("mine", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)10);
    QCOMPARE(key.GetValue_UInt32_IfOk("theirs", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)20);
    QCOMPARE(key.GetValue_UInt32_IfOk("shared", u), (LONG)ERROR_SUCCESS);
    QCOMPARE(u, (UInt32)3);
    QVERIFY(key.GetValue_UInt32_IfOk("gone", u) != ERROR_SUCCESS);
    CKey other;
    QCOMPARE(other.Open(HKEY_CURRENT_USER, "Software/7-Zip/Other", KEY_READ), (LONG)ERROR_SUCCESS);
  }

  void concurrentProcesses()
  {
    const int kWriters = 4;
    const int kCount = 200;
    QList<QProcess *> procs;
    for (int i = 0; i < kWriters; i++)
    {
      QProcess *p = new QProcess(this);
      p->setProcessChannelMode(QProcess::ForwardedChannels);
      p->start(QCoreApplication::applicationFilePath(),
          { "--writer", _ini, QString::number(i), QString::number(kCount) });
      procs.append(p);
    }
    for (QProcess *p : procs)
    {
      QVERIFY(p->waitForFinished(60000));
      QCOMPARE(p->exitStatus(), QProcess::NormalExit);
      QCOMPARE(p->exitCode(), 0);
    }
    Reopen();
    CKey key;
    QCOMPARE(key.Open(HKEY_CURRENT_USER, "Software/7-Zip/Shared", KEY_READ), (LONG)ERROR_SUCCESS);
    for (int w = 0; w < kWriters; w++)
      for (int i = 0; i < kCount; i++)
      {
        const QByteArray name = QString("w%1_%2").arg(w).arg(i).toUtf8();
        UInt32 u = 0;
        QVERIFY2(key.GetValue_UInt32_IfOk(name.constData(), u) == ERROR_SUCCESS, name.constData());
        QCOMPARE(u, (UInt32)i);
      }
    QVERIFY(QDir(QFileInfo(_ini).path()).entryList(QStringList() << "*.ini.*", QDir::Files).size() == 1); // only the lock file
  }

  void defaultPath()
  {
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_CONFIG_HOME", "/xdg/config");
    env.insert("HOME", "/home/someone");
    p.setProcessEnvironment(env);
    p.start(QCoreApplication::applicationFilePath(), { "--print-path" });
    QVERIFY(p.waitForFinished());
    QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()), QString("/xdg/config/7-Zip/7-Zip.ini\n"));

    env.insert("XDG_CONFIG_HOME", "relative/ignored");
    p.setProcessEnvironment(env);
    p.start(QCoreApplication::applicationFilePath(), { "--print-path" });
    QVERIFY(p.waitForFinished());
    QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()), QString("/home/someone/.config/7-Zip/7-Zip.ini\n"));
  }

  void zipRegistryRoundTrip()
  {
    {
      NExtract::CInfo e;
      e.PathMode = NExtract::NPathMode::kNoPaths;
      e.PathMode_Force = true;
      e.OverwriteMode = NExtract::NOverwriteMode::kRename;
      e.OverwriteMode_Force = true;
      e.SplitDest.Val = false; e.SplitDest.Def = true;
      e.ElimDup.Val = true; e.ElimDup.Def = true;
      e.NtSecurity.Val = false; e.NtSecurity.Def = false;
      e.ShowPassword.Val = true; e.ShowPassword.Def = true;
      e.Paths.Add(L"/home/user/out");
      e.Paths.Add(L"/tmp/\u0105");
      e.Save();
      NExtract::Save_LimitGB(12);

      NCompression::CInfo c;
      c.Load(); // defaults
      c.Level = 9;
      c.ArcType = L"7z";
      c.EncryptHeaders = true;
      c.ArcPaths.Add(L"/home/user/a.7z");
      NCompression::CFormatOptions fo;
      fo.FormatID = "7z";
      fo.Method = L"LZMA2";
      fo.Level = 7;
      fo.Dictionary = 1 << 26;
      fo.NumThreads = 4;
      fo.MemUse = L"80%";
      fo.MTime.Val = true; fo.MTime.Def = true;
      c.Formats.Add(fo);
      fo.FormatID = "zip";
      fo.Method = L"Deflate";
      c.Formats.Add(fo);
      c.Save();
    }
    Reopen();
    NExtract::CInfo e;
    e.Load();
    QCOMPARE((int)e.PathMode, (int)NExtract::NPathMode::kNoPaths);
    QVERIFY(e.PathMode_Force);
    QCOMPARE((int)e.OverwriteMode, (int)NExtract::NOverwriteMode::kRename);
    QVERIFY(e.SplitDest.Def && !e.SplitDest.Val);
    QVERIFY(e.ElimDup.Def && e.ElimDup.Val);
    QVERIFY(!e.NtSecurity.Def);
    QVERIFY(e.ShowPassword.Def && e.ShowPassword.Val);
    QCOMPARE((int)e.Paths.Size(), 2);
    QCOMPARE(ToQString(e.Paths[1]), QString::fromUtf8("/tmp/\u0105"));
    QCOMPARE(NExtract::Read_LimitGB(), (UInt32)12);
    QVERIFY(NExtract::Read_ShowPassword());

    NCompression::CInfo c;
    c.Load();
    QCOMPARE(c.Level, (UInt32)9);
    QCOMPARE(ToQString(c.ArcType), QString("7z"));
    QVERIFY(c.EncryptHeaders);
    QCOMPARE((int)c.ArcPaths.Size(), 1);
    QCOMPARE((int)c.Formats.Size(), 2);
    const NCompression::CFormatOptions &f = c.Formats[0];
    QCOMPARE(QString(f.FormatID.Ptr()), QString("7z"));
    QCOMPARE(ToQString(f.Method), QString("LZMA2"));
    QCOMPARE(f.Level, (UInt32)7);
    QCOMPARE(f.Dictionary, (UInt32)1 << 26);
    QCOMPARE(f.NumThreads, (UInt32)4);
    QCOMPARE(ToQString(f.MemUse), QString("80%"));
    QVERIFY(f.MTime.Def && f.MTime.Val);
    QCOMPARE(ToQString(c.Formats[1].Method), QString("Deflate"));
  }

  void fmSettingsRoundTrip()
  {
    {
      CFmSettings s;
      s.ShowDots = true;
      s.ShowRealFileIcons = false;
      s.FullRow = true;
      s.ShowGrid = true;
      s.SingleClick = false;
      s.AlternativeSelection = true;
      s.ShowSystemMenu = false;
      s.Save();
      SaveRegLang(L"pl");
      SaveRegEditor(true, L"/usr/bin/kate");
      SaveFlatView(1, true);
    }
    Reopen();
    CFmSettings s;
    s.Load();
    QVERIFY(s.ShowDots);
    QVERIFY(!s.ShowRealFileIcons);
    QVERIFY(s.FullRow);
    QVERIFY(s.ShowGrid);
    QVERIFY(!s.SingleClick);
    QVERIFY(s.AlternativeSelection);
    QVERIFY(!s.ShowSystemMenu);
    UString lang, editor, viewer;
    ReadRegLang(lang);
    QCOMPARE(ToQString(lang), QString("pl"));
    ReadRegEditor(true, editor);
    QCOMPARE(ToQString(editor), QString("/usr/bin/kate"));
    ReadRegEditor(false, viewer);
    QVERIFY(viewer.IsEmpty());
    QVERIFY(ReadFlatView(1));
    QVERIFY(!ReadFlatView(0));
  }

  void viewSettingsRoundTrip()
  {
    {
      CListViewInfo info;
      info.SortID = kpidMTime;
      info.Ascending = false;
      CColumnInfo col;
      col.PropID = kpidName; col.IsVisible = true; col.Width = 300;
      info.Columns.Add(col);
      col.PropID = kpidSize; col.IsVisible = false; col.Width = 80;
      info.Columns.Add(col);
      info.Save(L"7-Zip.zip");

      CWindowInfo w;
      w.rect.left = 10; w.rect.top = 20; w.rect.right = 810; w.rect.bottom = 620;
      w.maximized = true;
      w.numPanels = 2;
      w.currentPanel = 1;
      w.splitterPos = 400;
      w.Save();

      SaveToolbarsMask(5);
      CListMode mode;
      mode.Panels[0] = 1;
      mode.Panels[1] = 3;
      mode.Save();
      SavePanelPath(0, L"/home/user/");
      UStringVector folders;
      folders.Add(L"/a");
      folders.Add(L"/b");
      SaveFolderHistory(folders);
      SaveFastFolders(folders);
      SaveCopyHistory(folders);
    }
    Reopen();
    CListViewInfo info;
    info.Read(L"7-Zip.zip");
    QVERIFY(info.IsLoaded);
    QCOMPARE(info.SortID, (PROPID)kpidMTime);
    QVERIFY(!info.Ascending);
    QCOMPARE((int)info.Columns.Size(), 2);
    QCOMPARE(info.Columns[0].PropID, (PROPID)kpidName);
    QCOMPARE(info.Columns[0].Width, (UInt32)300);
    QVERIFY(!info.Columns[1].IsVisible);
    CListViewInfo other;
    other.Read(L"FSFolder");
    QVERIFY(!other.IsLoaded);

    CWindowInfo w;
    bool posDefined = false, panelDefined = false;
    w.Read(posDefined, panelDefined);
    QVERIFY(posDefined);
    QVERIFY(panelDefined);
    QCOMPARE((int)w.rect.right, 810);
    QVERIFY(w.maximized);
    QCOMPARE(w.numPanels, (UInt32)2);
    QCOMPARE(w.splitterPos, (UInt32)400);

    QCOMPARE(ReadToolbarsMask(), (UInt32)5);
    CListMode mode;
    mode.Read();
    QCOMPARE(mode.Panels[0], (UInt32)1);
    UString path;
    QVERIFY(ReadPanelPath(0, path));
    QCOMPARE(ToQString(path), QString("/home/user/"));
    for (int kind = 0; kind < 3; kind++)
    {
      UStringVector folders;
      if (kind == 0) ReadFolderHistory(folders);
      if (kind == 1) ReadFastFolders(folders);
      if (kind == 2) ReadCopyHistory(folders);
      QCOMPARE((int)folders.Size(), 2);
      QCOMPARE(ToQString(folders[1]), QString("/b"));
    }
  }
};

static int RunWriter(const char *ini, int id, int count)
{
  Ini_SetFilePath(FString(ini));
  for (int i = 0; i < count; i++)
  {
    char name[64];
    snprintf(name, sizeof(name), "w%d_%d", id, i);
    CKey key;
    if (key.Create(HKEY_CURRENT_USER, "Software/7-Zip/Shared") != ERROR_SUCCESS)
      return 1;
    if (key.SetValue(name, (UInt32)i) != ERROR_SUCCESS)
      return 2;
    if (key.Close() != ERROR_SUCCESS)
      return 3;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc == 2 && strcmp(argv[1], "--print-path") == 0)
  {
    printf("%s\n", Ini_GetFilePath().Ptr());
    return 0;
  }
  if (argc == 5 && strcmp(argv[1], "--writer") == 0)
    return RunWriter(argv[2], atoi(argv[3]), atoi(argv[4]));
  QCoreApplication app(argc, argv);
  CRegistryTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_registry.moc"
