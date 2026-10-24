# The dark theme of the 7-Zip programs: style, palette, the toolbar icons
# (SVG versions of the upstream toolbar bitmaps) and the application icon
# (the 7-Zip icon of FileManager/FM.ico as SVG; linux/packaging installs
# it in the hicolor theme).

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_library(z7_theme STATIC Theme.cpp)
z7_port_target(z7_theme)
target_include_directories(z7_theme PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Svg: the SVG icon engine reads the toolbar icons
target_link_libraries(z7_theme PUBLIC Qt6::Widgets Qt6::Svg)

set(Z7_TOOLBAR_ICONS Add Extract Test Copy Move Delete Info)
set(Z7_TOOLBAR_FILES icons/toolbar/ParentFolder.svg)
foreach(name IN LISTS Z7_TOOLBAR_ICONS)
  list(APPEND Z7_TOOLBAR_FILES icons/toolbar/${name}.svg icons/toolbar/${name}2.svg)
endforeach()
qt_add_resources(z7_theme "z7_theme_icons" PREFIX "/z7/toolbar" BASE icons/toolbar
  FILES ${Z7_TOOLBAR_FILES})
qt_add_resources(z7_theme "z7_theme_app_icon" PREFIX "/z7/app" BASE icons/app
  FILES icons/app/7-Zip.svg)
