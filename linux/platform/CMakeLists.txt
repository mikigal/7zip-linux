# Platform layer for 7zFM and 7zG.
#
# z7_ui: upstream code shared by the GUI programs (UI/Common, UI/Agent and the
# non-window parts of UI/FileManager), compiled unchanged with Win32Compat.h
# force-included.
#
# z7_platform: Linux implementations of the Windows-only pieces that code
# needs (registry, resource strings, locale, POSIX folders).
#
# Programs link both through z7_link_ui(), which also links the z7_core
# objects directly (handlers and codecs register themselves from static
# initializers, so they must not go through a static library).

set(Z7_COMPAT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Win32Compat.h")

# Upstream sources beyond the 7zz list, from UI_COMMON_OBJS, AGENT_OBJS and
# FM_OBJS in CPP/7zip/Bundles/Fm/makefile and UI/FileManager/FM.mak.
set(Z7_UI_EXTRA_SOURCES
  CPP/Common/Lang.cpp
  CPP/7zip/UI/Common/ArchiveName.cpp
  CPP/7zip/UI/Common/WorkDir.cpp
  CPP/7zip/UI/Common/ZipRegistry.cpp
  CPP/7zip/UI/Agent/Agent.cpp
  CPP/7zip/UI/Agent/AgentOut.cpp
  CPP/7zip/UI/Agent/AgentProxy.cpp
  CPP/7zip/UI/Agent/ArchiveFolder.cpp
  CPP/7zip/UI/Agent/ArchiveFolderOpen.cpp
  CPP/7zip/UI/Agent/ArchiveFolderOut.cpp
  CPP/7zip/UI/Agent/UpdateCallbackAgent.cpp
  CPP/7zip/UI/FileManager/RegistryUtils.cpp
  CPP/7zip/UI/FileManager/ViewSettings.cpp
)

z7_upstream_paths(Z7_UI_SOURCES ${Z7_7ZZ_UI_COMMON_SOURCES} ${Z7_UI_EXTRA_SOURCES})

add_library(z7_ui STATIC ${Z7_UI_SOURCES})
target_compile_definitions(z7_ui PUBLIC ${Z7_BASE_DEFINITIONS} Z7_LANG)
target_compile_options(z7_ui PRIVATE ${Z7_UPSTREAM_WARNINGS} -include "${Z7_COMPAT_HEADER}")

add_library(z7_platform STATIC
  ClassDefs.cpp
  Registry.cpp
)
z7_port_target(z7_platform)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(z7_platform PRIVATE -Wno-non-virtual-dtor)
target_compile_definitions(z7_platform PUBLIC ${Z7_BASE_DEFINITIONS} Z7_LANG)
target_include_directories(z7_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The two libraries reference each other; CMake repeats them on the link line.
target_link_libraries(z7_ui PUBLIC z7_platform)
target_link_libraries(z7_platform PUBLIC z7_ui)

# z7_link_ui(<target>): links the platform layer, the shared UI code and the
# 7-Zip core into <target>.
function(z7_link_ui target)
  target_link_libraries(${target} PRIVATE z7_platform z7_ui z7_core)
endfunction()
