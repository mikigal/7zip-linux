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
