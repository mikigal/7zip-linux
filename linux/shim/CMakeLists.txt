# The Qt implementation of the Win32 user32 subset that the 7-Zip GUI code
# uses (z7_shim), and the upstream code that runs on top of it (z7_gui).
#
# z7_gui: upstream sources compiled unchanged: the NWindows wrappers
# (Window.cpp, Control/*.cpp) and the GUI code shared by 7zFM and 7zG. They
# see the stub Windows headers in win32/ first on the include path, with
# windows.h force-included, as <windows.h> is on Windows.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(Z7_SHIM_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/win32")

add_library(z7_shim STATIC
  ShimControls.cpp
  ShimCore.cpp
  ShimDialog.cpp
  ShimListView.cpp
  ShimMessageBox.cpp
  ShimPropSheet.cpp
  ShimResources.cpp
)
z7_port_target(z7_shim)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(z7_shim PRIVATE -Wno-non-virtual-dtor)
target_include_directories(z7_shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${Z7_SHIM_INCLUDE}")
target_link_libraries(z7_shim PUBLIC Qt6::Widgets z7_platform)

set(Z7_GUI_SOURCES
  CPP/Windows/Window.cpp
  CPP/Windows/Control/ComboBox.cpp
  CPP/Windows/Control/Dialog.cpp
  CPP/Windows/Control/ListView.cpp
  CPP/Windows/Control/PropertyPage.cpp
  CPP/Windows/Control/Window2.cpp
  CPP/7zip/UI/FileManager/LangUtils.cpp
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
