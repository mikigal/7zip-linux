// FolderCompare.cpp -- CompareFileNames_ForFolderList() (declared in
// FileManager/IFolder.h), which UI/Agent and the folders use for sorting.
//
// This is a copy of the function in FileManager/PanelSort.cpp, which needs
// the panel and cannot be built yet; unit_folders checks that both copies
// are the same. Remove this file when PanelSort.cpp is built.

#include "Win32Compat.h"

#include "../../CPP/Common/MyString.h"

#include "../../CPP/7zip/UI/FileManager/IFolder.h"

int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2)
{
  for (;;)
  {
    wchar_t c1 = *s1;
    wchar_t c2 = *s2;
    if ((c1 >= '0' && c1 <= '9') &&
        (c2 >= '0' && c2 <= '9'))
    {
      for (; *s1 == '0'; s1++);
      for (; *s2 == '0'; s2++);
      size_t len1 = 0;
      size_t len2 = 0;
      for (; (s1[len1] >= '0' && s1[len1] <= '9'); len1++);
      for (; (s2[len2] >= '0' && s2[len2] <= '9'); len2++);
      if (len1 < len2) return -1;
      if (len1 > len2) return 1;
      for (; len1 > 0; s1++, s2++, len1--)
      {
        if (*s1 == *s2) continue;
        return (*s1 < *s2) ? -1 : 1;
      }
      c1 = *s1;
      c2 = *s2;
    }
    s1++;
    s2++;
    if (c1 != c2)
    {
      // Probably we need to change the order for special characters like in Explorer.
      wchar_t u1 = MyCharUpper(c1);
      wchar_t u2 = MyCharUpper(c2);
      if (u1 < u2) return -1;
      if (u1 > u2) return 1;
    }
    if (c1 == 0) return 0;
  }
}
