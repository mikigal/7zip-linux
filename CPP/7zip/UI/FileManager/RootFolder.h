// RootFolder.h

#ifdef Z7_QT_GUI
// Linux GUI port: the POSIX root folder (same class name).
#include "../../../../linux/platform/RootFolderPosix.h"
#else

#ifndef ZIP7_INC_ROOT_FOLDER_H
#define ZIP7_INC_ROOT_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"

#include "IFolder.h"

const unsigned kNumRootFolderItems_Max = 4;

Z7_CLASS_IMP_NOQIB_2(
  CRootFolder
  , IFolderFolder
  , IFolderGetSystemIconIndex
)
  UString _names[kNumRootFolderItems_Max];
  int _iconIndices[kNumRootFolderItems_Max];
public:
  void Init();
};

#endif

#endif
