# 7zG: the 7-Zip dialogs program (compress, extract, test, hash, benchmark)
# that file manager context menus run. GUI/GUI.cpp is the upstream main
# source; Main.cpp sets up Qt and the platform layer and calls its WinMain().

# GUI.cpp is upstream code, so it is built with the upstream warning level.
add_library(z7_7zg_main OBJECT "${Z7_ROOT}/CPP/7zip/UI/GUI/GUI.cpp")
target_compile_options(z7_7zg_main PRIVATE ${Z7_UPSTREAM_WARNINGS} -include "${Z7_SHIM_INCLUDE}/windows.h")
target_link_libraries(z7_7zg_main PRIVATE z7_gui)

add_executable(7zG Main.cpp)
z7_port_target(7zG)
# Upstream COM interfaces have no virtual destructors by design.
target_compile_options(7zG PRIVATE -Wno-non-virtual-dtor)
z7_link_ui(7zG GUI)
target_link_libraries(7zG PRIVATE z7_7zg_main z7_gui z7_theme)
