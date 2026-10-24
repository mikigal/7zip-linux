// Locale.cpp -- the user's language for 7-Zip language files, and the
// program directory.

#include "Win32Compat.h"

#include <stdlib.h>
#include <unistd.h>

#include "../../CPP/Common/StringConvert.h"

#include "Platform.h"

// UI/Common/ArchiveCommandLine.cpp (POSIX part)
void Set_ModuleDirPrefix_From_ProgArg0(const char *s);

namespace NPlatform {

/* The same list as kLangs in FileManager/LangUtils.cpp: the position of a
   plain entry is the Windows primary language ID; "-xx" entries are the
   sublanguages "<main>-xx" and "=xx" entries the sublanguages "xx" (the
   first sublanguage has SUBLANGID 1). unit_resources checks that both lists
   are equal. */
static const char * const kLangs =
  "ar.bg.ca.zh.-tw.-cn.cs.da.de.el.en.es.fi.fr.he.hu.is."
  "it.ja.ko.nl.no.=nb.=nn.pl.pt.-br.rm.ro.ru.sr.=hr.-spl.-spc.=hr.=bs.sk.sq.sv.th.tr."
  "ur.id.uk.be.sl.et.lv.lt.tg.fa.vi.hy.az.eu.hsb.mk."
  "st.ts.tn.ve.xh.zu.af.ka.fo.hi.mt.se.ga.yi.ms.kk."
  "ky.sw.tk.uz.-latn.-cyrl.tt.bn.pa.-in.gu.or.ta.te.kn.ml.as.mr.sa."
  "mn.=mn.=mng.bo.cy.kh.lo.my.gl.kok..sd.syr.si..iu.am.tzm."
  "ks.ne.fy.ps.tl.dv..ff.ha..yo.qu.st.ba.lb.kl."
  "ig.kr.om.ti.gn..la.so.ii..arn..moh..br.."
  "ug.mi.oc.co.";

// Locale names (lowercase, '_' before the country) that need a specific
// 7-Zip name, tried before the plain language.
static const char * const kAliases[][2] =
{
  { "zh_tw", "zh-tw" },
  { "zh_hk", "zh-tw" },
  { "zh_mo", "zh-tw" },
  { "zh", "zh-cn" },
  { "pt_br", "pt-br" },
  { "pa_in", "pa-in" },
  { "sr@latin", "sr-spl" },
  { "sr", "sr-spc" },
  { "uz@cyrillic", "uz-cyrl" },
  { "uz", "uz-latn" },
  { "km", "kh" }
};

static const LANGID kEnglishUS = MAKELANGID(0x09, 0x01);

// Finds a 7-Zip language name in kLangs.
static bool FindLangName(const AString &name, LANGID &langId)
{
  unsigned primary = 0;
  unsigned sub = 0;
  AString main;
  for (const char *p = kLangs; *p != 0;)
  {
    const char *end = p;
    while (*end != '.')
      end++;
    AString entry;
    entry.SetFrom(p, (unsigned)(end - p));
    p = end + 1;
    const bool isSub = (!entry.IsEmpty() && (entry[0] == '-' || entry[0] == '='));
    if (!isSub)
    {
      primary++;
      sub = 0;
      main = entry;
      if (!entry.IsEmpty() && entry == name)
      {
        // A plain name selects the main language file: SUBLANG_DEFAULT
        // unless the language has sublanguage files, else SUBLANG_NEUTRAL.
        sub = (*p == '-' || *p == '=') ? 0 : 1;
        langId = MAKELANGID(primary, sub);
        return true;
      }
      continue;
    }
    sub++;
    AString subName;
    if (entry[0] == '-')
    {
      subName = main;
      subName += entry;
    }
    else
      subName = entry.Ptr(1);
    if (subName == name)
    {
      langId = MAKELANGID(primary, sub);
      return true;
    }
  }
  return false;
}

LANGID LangIdFromLocale(const char *locale)
{
  // "ll_CC.codeset@modifier"
  AString lang, country, modifier;
  const char *p = locale ? locale : "";
  for (; *p != 0 && *p != '_' && *p != '.' && *p != '@'; p++)
    lang.Add_Char(*p);
  if (*p == '_')
    for (p++; *p != 0 && *p != '.' && *p != '@'; p++)
      country.Add_Char(*p);
  while (*p != 0 && *p != '@')
    p++;
  if (*p == '@')
    modifier = p + 1;
  lang.MakeLower_Ascii();
  country.MakeLower_Ascii();
  modifier.MakeLower_Ascii();
  if (lang.IsEmpty() || lang == "c" || lang == "posix")
    return kEnglishUS;

  AStringVector candidates;
  if (!modifier.IsEmpty())
  {
    if (!country.IsEmpty())
      candidates.Add(lang + "_" + country + "@" + modifier);
    candidates.Add(lang + "@" + modifier);
  }
  if (!country.IsEmpty())
    candidates.Add(lang + "_" + country);
  candidates.Add(lang);

  FOR_VECTOR (i, candidates)
  {
    const AString &c = candidates[i];
    for (unsigned k = 0; k < Z7_ARRAY_SIZE(kAliases); k++)
      if (c == kAliases[k][0])
      {
        LANGID langId;
        if (FindLangName(AString(kAliases[k][1]), langId))
          return langId;
      }
    if (c == lang)
    {
      LANGID langId;
      if (FindLangName(c, langId))
        return langId;
    }
  }
  return kEnglishUS;
}

static const char *GetEnvNonEmpty(const char *name)
{
  const char *s = getenv(name);
  return (s && *s) ? s : NULL;
}

AString GetMessagesLocale()
{
  const char *locale = GetEnvNonEmpty("LC_ALL");
  if (!locale)
    locale = GetEnvNonEmpty("LC_MESSAGES");
  if (!locale)
    locale = GetEnvNonEmpty("LANG");
  AString res (locale ? locale : "C");
  // GNU gettext: $LANGUAGE has priority, except in the "C" locale
  const char *language = GetEnvNonEmpty("LANGUAGE");
  if (language && res != "C" && res != "POSIX")
  {
    res.Empty();
    for (; *language != 0 && *language != ':'; language++)
      res.Add_Char(*language);
  }
  return res;
}

void InitModuleDirPrefix(const char *argv0)
{
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0)
  {
    buf[n] = 0;
    Set_ModuleDirPrefix_From_ProgArg0(buf);
  }
  else if (argv0)
    Set_ModuleDirPrefix_From_ProgArg0(argv0);
}

}

LANGID GetUserDefaultLangID()
{
  return NPlatform::LangIdFromLocale(NPlatform::GetMessagesLocale());
}

LANGID GetSystemDefaultLangID()
{
  // LangUtils.cpp uses the language only if both IDs have the same primary
  // language; there is no separate system language here.
  return GetUserDefaultLangID();
}
