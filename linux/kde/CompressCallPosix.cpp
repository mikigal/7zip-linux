// CompressCallPosix.cpp -- UI/Common/CompressCall.h for the context menu of
// file managers: as CompressCall.cpp does on Windows, the commands run 7zG
// from the program folder, with the arguments of linux/gui/GuiCommand.h.

#include <QProcess>
#include <QStringList>

#include <windows.h>

#include "../../CPP/Common/MyString.h"
#include "../../CPP/Common/StringConvert.h"

#include "../../CPP/Windows/DLL.h"

#include "../../CPP/7zip/UI/Common/CompressCall.h"

#include "GuiCommand.h"
#include "ShimWindow.h"

UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2.Add_Char('\"');
  return s2;
}

// 7zG runs on its own (detached), as CreateProcess() starts it on Windows.
static HRESULT Call7zGui(const UStringVector &params)
{
  const QString program = NShim::FromU(fs2us(NWindows::NDLL::GetModuleDirPrefix())) + QStringLiteral("7zG");
  QStringList args;
  FOR_VECTOR (i, params)
    args.append(NShim::FromU(params[i]));
  if (QProcess::startDetached(program, args))
    return S_OK;
  const QString message = QStringLiteral("Cannot start %1").arg(program);
  MessageBoxW(NULL, (LPCWSTR)NShim::ToU(message), L"7-Zip", MB_ICONERROR | MB_OK);
  return E_FAIL;
}

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool /* waitFinish */)
{
  UStringVector params;
  NGuiCommand::Compress(params, arcName.IsEmpty() ? UString() : arcPathPrefix + arcName,
      arcType, addExtension, names, email, showDialog);
  return Call7zGui(params);
}

void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog,
    bool elimDup, UInt32 /* writeZone */)
{
  UStringVector params;
  NGuiCommand::Extract(params, arcPaths, outFolder, showDialog, elimDup);
  Call7zGui(params);
}

void TestArchives(const UStringVector &arcPaths, bool hashMode)
{
  UStringVector params;
  NGuiCommand::Test(params, arcPaths, hashMode);
  Call7zGui(params);
}

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName)
{
  // a checksum file is an archive of the "hash" type, as on Windows
  if (!arcFileName.IsEmpty())
  {
    CompressFiles(arcPathPrefix, arcFileName, UString("hash"),
        false, // addExtension
        paths,
        false, // email
        false, // showDialog
        false); // waitFinish
    return;
  }
  UStringVector params;
  NGuiCommand::Hash(params, paths, methodName);
  Call7zGui(params);
}

void Benchmark(bool totalMode)
{
  UStringVector params;
  NGuiCommand::Benchmark(params, totalMode);
  Call7zGui(params);
}
