# The Qt implementation of the Win32 user32 subset that the 7-Zip GUI code
# uses (z7_shim), and the upstream code that runs on top of it (z7_gui).
#
# z7_gui: upstream sources compiled unchanged: the NWindows wrappers
# (Window.cpp, Control/*.cpp) and the dialogs shared by 7zFM and 7zG. They
# see the stub Windows headers in win32/ first on the include path, with
# windows.h force-included, as <windows.h> is on Windows.

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)

set(Z7_SHIM_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/win32" CACHE INTERNAL "Stub Windows headers of linux/shim")

add_library(z7_shim STATIC
  ShimBars.cpp
  ShimControls.cpp
  ShimCore.cpp
  ShimDialog.cpp
  ShimDragDrop.cpp
  ShimGuids.cpp
  ShimInput.cpp
  ShimListView.cpp
  ShimMenu.cpp
  ShimMessageBox.cpp
  ShimPropSheet.cpp
  ShimResources.cpp
  ShimShell.cpp
  ShimShellFolder.cpp
  ShimSystem.cpp
)
z7_port_target(z7_shim)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(z7_shim PRIVATE -Wno-non-virtual-dtor)
target_include_directories(z7_shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_shim PUBLIC Qt6::Widgets Qt6::DBus z7_platform)

# From GUI_OBJS, FM_OBJS, EXPLORER_OBJS and WIN_CTRL_OBJS in
# CPP/7zip/UI/GUI/makefile; 7zFM (Bundles/Fm) has these dialogs too. The
# Win32-only WIN_OBJS (Clipboard, CommonDialog, Shell) are implemented in
# ShimShell.cpp.
set(Z7_GUI_SOURCES
  CPP/Windows/Window.cpp
  CPP/Windows/Control/ComboBox.cpp
  CPP/Windows/Control/Dialog.cpp
  CPP/Windows/Control/ListView.cpp
  CPP/Windows/Control/PropertyPage.cpp
  CPP/Windows/Control/Window2.cpp
  CPP/Windows/Menu.cpp
  CPP/7zip/UI/Explorer/MyMessages.cpp
  CPP/7zip/UI/FileManager/BrowseDialog.cpp
  CPP/7zip/UI/FileManager/ComboDialog.cpp
  CPP/7zip/UI/FileManager/EditDialog.cpp
  CPP/7zip/UI/FileManager/ExtractCallback.cpp
  CPP/7zip/UI/FileManager/FormatUtils.cpp
  CPP/7zip/UI/FileManager/HelpUtils.cpp
  CPP/7zip/UI/FileManager/LangUtils.cpp
  CPP/7zip/UI/FileManager/ListViewDialog.cpp
  CPP/7zip/UI/FileManager/MemDialog.cpp
  CPP/7zip/UI/FileManager/OpenCallback.cpp
  CPP/7zip/UI/FileManager/OverwriteDialog.cpp
  CPP/7zip/UI/FileManager/PasswordDialog.cpp
  CPP/7zip/UI/FileManager/ProgramLocation.cpp
  CPP/7zip/UI/FileManager/ProgressDialog2.cpp
  CPP/7zip/UI/FileManager/PropertyName.cpp
  CPP/7zip/UI/FileManager/SplitUtils.cpp
  CPP/7zip/UI/FileManager/StringUtils.cpp
  CPP/7zip/UI/FileManager/SysIconUtils.cpp
  CPP/7zip/UI/GUI/BenchmarkDialog.cpp
  CPP/7zip/UI/GUI/CompressDialog.cpp
  CPP/7zip/UI/GUI/ExtractDialog.cpp
  CPP/7zip/UI/GUI/ExtractGUI.cpp
  CPP/7zip/UI/GUI/HashGUI.cpp
  CPP/7zip/UI/GUI/UpdateCallbackGUI.cpp
  CPP/7zip/UI/GUI/UpdateCallbackGUI2.cpp
  CPP/7zip/UI/GUI/UpdateGUI.cpp
)
z7_upstream_paths(Z7_GUI_SOURCES ${Z7_GUI_SOURCES})

add_library(z7_gui STATIC ${Z7_GUI_SOURCES})
target_compile_definitions(z7_gui PUBLIC ${Z7_BASE_DEFINITIONS} Z7_LANG)
target_compile_options(z7_gui PRIVATE ${Z7_UPSTREAM_WARNINGS} -include "${Z7_SHIM_INCLUDE}/windows.h")
target_include_directories(z7_gui PRIVATE "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_gui PUBLIC z7_shim)

# z7_upstream_gui_sources(<absolute paths...>): compile options for upstream
# GUI sources (dialogs) that a program target compiles against the shim.
function(z7_upstream_gui_sources)
  foreach(src IN LISTS ARGN)
    set_property(SOURCE "${src}" APPEND PROPERTY COMPILE_OPTIONS
      ${Z7_UPSTREAM_WARNINGS} -include "${Z7_SHIM_INCLUDE}/windows.h")
  endforeach()
endfunction()
