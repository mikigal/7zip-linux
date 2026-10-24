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

# winref: a Winelib program that renders the dialog templates of the 7-Zip
# release for Windows with Wine, for reference pictures (see
# winref/wine_reference.py). Built on request: cmake --build ... --target winref
find_program(Z7_WINEGCC winegcc)
find_program(Z7_WRC wrc)
if(Z7_WINEGCC AND Z7_WRC)
  set(Z7_WINREF_DIR "${CMAKE_CURRENT_BINARY_DIR}/winref")
  add_custom_command(OUTPUT "${Z7_WINREF_DIR}/winref.exe" "${Z7_WINREF_DIR}/winref.exe.so"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${Z7_WINREF_DIR}"
    COMMAND "${Z7_WRC}" -o "${Z7_WINREF_DIR}/winref.res" winref.rc
    COMMAND "${Z7_WINEGCC}" -m64 -municode -mwindows -Wall -Wextra -Werror
            -o "${Z7_WINREF_DIR}/winref.exe" winref.c "${Z7_WINREF_DIR}/winref.res"
            -lcomctl32 -luser32 -lgdi32
    DEPENDS winref/winref.c winref/winref.rc winref/winref.manifest
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/winref"
    COMMENT "Building winref with winegcc"
    VERBATIM)
  add_custom_target(winref DEPENDS "${Z7_WINREF_DIR}/winref.exe")
endif()
