# Developer and test tools.

find_package(Qt6 COMPONENTS Core DBus)
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBEI IMPORTED_TARGET libei-1.0)
endif()

# ei-input: input injection into the nested KWin used by the Wayland tests.
if(Qt6_FOUND AND LIBEI_FOUND)
  add_executable(ei-input eiinput/ei_input.cpp)
  z7_port_target(ei-input)
  target_link_libraries(ei-input PRIVATE Qt6::Core Qt6::DBus PkgConfig::LIBEI)
else()
  message(STATUS "ei-input disabled (needs Qt6 DBus and libei-1.0)")
endif()

# z7-gallery: renders the dialog templates with the dark theme (see
# tests/render).
if(TARGET z7_gui AND TARGET z7_theme)
  add_executable(z7-gallery gallery/gallery.cpp)
  z7_port_target(z7-gallery)
  target_compile_options(z7-gallery PRIVATE -Wno-non-virtual-dtor)
  z7_link_ui(z7-gallery FM)
  target_link_libraries(z7-gallery PRIVATE z7_gui z7_theme)
endif()
