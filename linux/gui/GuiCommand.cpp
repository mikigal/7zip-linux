// GuiCommand.cpp

#include "GuiCommand.h"

namespace NGuiCommand {

// the -i switch of CompressCall.cpp, without wildcards ("w-"), for one name
static void AddNames(UStringVector &args, const char *switchName, const UStringVector &names)
{
  FOR_VECTOR (i, names)
  {
    UString s (switchName);
    s += "w-!";
    s += names[i];
    args.Add(s);
  }
}

void Compress(UStringVector &args, const UString &arcPath, const UString &arcType,
    bool addExtension, const UStringVector &names, bool email, bool showDialog)
{
  args.Clear();
  args.Add(UString("a"));
  AddNames(args, "-i", names);
  if (!arcType.IsEmpty())
    args.Add(UString("-t") + arcType);
  if (email)
    args.Add(UString("-seml."));
  if (showDialog)
    args.Add(UString("-ad"));
  if (arcPath.IsEmpty())
    args.Add(UString("-an"));
  args.Add(UString(addExtension ? "-saa" : "-sae"));
  args.Add(UString("--"));
  if (!arcPath.IsEmpty())
    args.Add(arcPath);
}

void Extract(UStringVector &args, const UStringVector &arcPaths, const UString &outFolder,
    bool showDialog, bool elimDup)
{
  args.Clear();
  args.Add(UString("x"));
  if (!outFolder.IsEmpty())
    args.Add(UString("-o") + outFolder);
  if (elimDup)
    args.Add(UString("-spe"));
  if (showDialog)
    args.Add(UString("-ad"));
  args.Add(UString("-an"));
  AddNames(args, "-ai", arcPaths);
}

void Test(UStringVector &args, const UStringVector &arcPaths, bool hashMode)
{
  args.Clear();
  args.Add(UString("t"));
  if (hashMode)
    args.Add(UString("-thash"));
  args.Add(UString("-an"));
  AddNames(args, "-ai", arcPaths);
}

void Hash(UStringVector &args, const UStringVector &paths, const UString &methodName)
{
  args.Clear();
  args.Add(UString("h"));
  if (!methodName.IsEmpty())
    args.Add(UString("-scrc") + methodName);
  AddNames(args, "-i", paths);
}

void Benchmark(UStringVector &args, bool totalMode)
{
  args.Clear();
  args.Add(UString("b"));
  if (totalMode)
    args.Add(UString("-mm=*"));
}

}
