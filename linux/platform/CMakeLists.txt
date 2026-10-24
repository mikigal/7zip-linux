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

# From the 7zz list: UI/Common and its support code (streams, command line
# parser, error messages), everything except the core and the console UI.
set(Z7_UI_7ZZ_SOURCES ${Z7_7ZZ_SOURCES})
list(REMOVE_ITEM Z7_UI_7ZZ_SOURCES
  ${Z7_7ZZ_ARC_SOURCES} ${Z7_7ZZ_SYS_SOURCES} ${Z7_7ZZ_CONSOLE_SOURCES})

z7_upstream_paths(Z7_UI_SOURCES ${Z7_UI_7ZZ_SOURCES} ${Z7_UI_EXTRA_SOURCES})

add_library(z7_ui STATIC ${Z7_UI_SOURCES})
target_compile_definitions(z7_ui PUBLIC ${Z7_BASE_DEFINITIONS} Z7_LANG)
target_compile_options(z7_ui PRIVATE ${Z7_UPSTREAM_WARNINGS} -include "${Z7_COMPAT_HEADER}")

add_library(z7_platform STATIC
  ClassDefs.cpp
  Globals.cpp
  Locale.cpp
  RcTables.cpp
  Registry.cpp
  ResourceString.cpp
)
z7_port_target(z7_platform)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(z7_platform PRIVATE -Wno-non-virtual-dtor)
target_compile_definitions(z7_platform PUBLIC ${Z7_BASE_DEFINITIONS} Z7_LANG)
target_include_directories(z7_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The two libraries reference each other; CMake repeats them on the link line.
target_link_libraries(z7_ui PUBLIC z7_platform)
target_link_libraries(z7_platform PUBLIC z7_ui)

# Resource tables generated from the upstream .rc files by tools/rcgen.py:
# z7_rc_fm for 7zFM (Bundles/Fm/resource.rc) and z7_rc_gui for 7zG
# (UI/GUI/resource.rc). Each defines NRc::g_Resources.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# z7_rc_library(<library> <generated file name> <resource script>)
function(z7_rc_library lib name script)
  set(out "${CMAKE_CURRENT_BINARY_DIR}/${name}")
  add_custom_command(OUTPUT "${out}"
    COMMAND Python3::Interpreter -I "${Z7_ROOT}/linux/tools/rcgen.py" "${Z7_ROOT}/${script}"
            --cpp "${out}" --depfile "${out}.d" --cc "${CMAKE_C_COMPILER}"
    DEPENDS "${Z7_ROOT}/linux/tools/rcgen.py" "${Z7_ROOT}/${script}"
    DEPFILE "${out}.d"
    COMMENT "Generating resource tables from ${script}"
    VERBATIM)
  add_library(${lib} STATIC "${out}")
  z7_port_target(${lib})
  target_include_directories(${lib} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

z7_rc_library(z7_rc_fm RcFm.cpp CPP/7zip/Bundles/Fm/resource.rc)
z7_rc_library(z7_rc_gui RcGui.cpp CPP/7zip/UI/GUI/resource.rc)

# z7_link_ui(<target> FM|GUI): links the platform layer, the shared UI code,
# the resource tables of 7zFM or 7zG and the 7-Zip core into <target>.
function(z7_link_ui target program)
  string(TOLOWER "${program}" program)
  target_link_libraries(${target} PRIVATE z7_platform z7_ui z7_rc_${program} z7_core)
endfunction()
