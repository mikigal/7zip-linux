find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME upstream_sources_current
  COMMAND Python3::Interpreter -I "${Z7_ROOT}/linux/tools/upstream_sources.py" --check)

add_test(NAME rcgen
  COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/rcgen/test_rcgen.py")
set_tests_properties(rcgen PROPERTIES LABELS "unit" TIMEOUT 120
  ENVIRONMENT "CC=${CMAKE_C_COMPILER}")

if(Z7_BUILD_ORACLE)
  add_test(NAME core_roundtrip
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/core/test_roundtrip.py"
            --ours $<TARGET_FILE:7zz> --oracle "${Z7_ORACLE_7ZZ}")
  set_tests_properties(core_roundtrip PROPERTIES LABELS "core" TIMEOUT 300)
endif()

# The compiled dialog templates of the 7-Zip release for Windows against the
# rcgen tables, when an extracted release is configured (see README.md).
set(Z7_WINDOWS_RELEASE "" CACHE PATH "Extracted 7-Zip release for Windows (7zFM.exe, 7zG.exe), for reference checks")
if(Z7_WINDOWS_RELEASE)
  add_test(NAME rcgen_windows_release
    COMMAND Python3::Interpreter -I "${Z7_ROOT}/linux/tools/winref/wine_reference.py"
            --release "${Z7_WINDOWS_RELEASE}" --templates)
  set_tests_properties(rcgen_windows_release PROPERTIES LABELS "unit" TIMEOUT 120)
endif()

# Unit tests (label "unit"): QtTest programs, run with a private HOME and
# XDG_CONFIG_HOME so that they never see the user's settings.
find_package(Qt6 COMPONENTS Core Test Widgets)
set(Z7_UNIT_HOME "${CMAKE_BINARY_DIR}/test-artifacts/unit-home")

# z7_unit_test(<name> <sources...>): a test program linked with the platform
# layer, registered as the ctest test "unit_<name>".
function(z7_unit_test name)
  add_executable(z7-test-${name} ${ARGN})
  z7_port_target(z7-test-${name})
  target_compile_options(z7-test-${name} PRIVATE -Wno-non-virtual-dtor)
  set_target_properties(z7-test-${name} PROPERTIES AUTOMOC ON)
  z7_link_ui(z7-test-${name} FM)
  target_link_libraries(z7-test-${name} PRIVATE Qt6::Core Qt6::Test)
  target_include_directories(z7-test-${name} PRIVATE "${Z7_ROOT}/linux/shim/win32")
  target_compile_definitions(z7-test-${name} PRIVATE Z7_SOURCE_ROOT="${Z7_ROOT}")
  add_test(NAME unit_${name} COMMAND z7-test-${name})
  set_tests_properties(unit_${name} PROPERTIES LABELS "unit" TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${Z7_UNIT_HOME};XDG_CONFIG_HOME=${Z7_UNIT_HOME}/config;XDG_DATA_HOME=${Z7_UNIT_HOME}/data;XDG_CACHE_HOME=${Z7_UNIT_HOME}/cache;XDG_STATE_HOME=${Z7_UNIT_HOME}/state")
endfunction()

if(Qt6Test_FOUND)
  file(MAKE_DIRECTORY "${Z7_UNIT_HOME}")
  z7_unit_test(registry unit/test_registry.cpp)
  z7_unit_test(resources unit/test_resources.cpp)
  z7_unit_test(folders unit/test_folders.cpp)
  # archives for the tests are made with the 7zz of this build
  target_compile_definitions(z7-test-folders PRIVATE Z7_TEST_7ZZ="$<TARGET_FILE:7zz>")
  add_dependencies(z7-test-folders 7zz)
  z7_unit_test(shim unit/test_shim.cpp)
  target_link_libraries(z7-test-shim PRIVATE z7_gui Qt6::Widgets)
else()
  message(STATUS "Unit tests disabled (need Qt6 Test)")
endif()

# z7-gui-driver: 7zG with the dialogs answered by a script (gui/gui_driver.cpp),
# for the end-to-end tests of 7zG.
if(TARGET 7zG)
  add_executable(z7-gui-driver gui/gui_driver.cpp)
  z7_port_target(z7-gui-driver)
  # Upstream COM interfaces have no virtual destructors by design.
  target_compile_options(z7-gui-driver PRIVATE -Wno-non-virtual-dtor)
  z7_link_ui(z7-gui-driver GUI)
  target_link_libraries(z7-gui-driver PRIVATE z7_7zg_main z7_gui z7_theme)

  if(Z7_BUILD_ORACLE)
    add_test(NAME gui_7zg
      COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/gui/test_7zg.py"
              --driver $<TARGET_FILE:z7-gui-driver> --oracle "${Z7_ORACLE_7ZZ}"
              --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/gui_7zg")
    set_tests_properties(gui_7zg PROPERTIES LABELS "gui" TIMEOUT 600)
  endif()
endif()

find_program(Z7_MAGICK magick)

# Render tests (label "render"): every dialog of the gallery against the
# golden pictures in render/golden.
if(TARGET z7-gallery AND Z7_MAGICK)
  add_test(NAME render_gallery
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/render/test_gallery.py"
            --gallery $<TARGET_FILE:z7-gallery>
            --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/render_gallery")
  set_tests_properties(render_gallery PROPERTIES LABELS "render" TIMEOUT 180 SKIP_RETURN_CODE 77)
else()
  message(STATUS "Render tests disabled (need z7-gallery and ImageMagick)")
endif()

# Wayland scenario tests run in a nested headless KWin (label "wayland").
find_package(Qt6 COMPONENTS Widgets)
find_program(Z7_KWIN kwin_wayland)
find_program(Z7_SPECTACLE spectacle)

if(Qt6Widgets_FOUND)
  add_executable(z7-probe wayland/probe_window.cpp)
  z7_port_target(z7-probe)
  target_link_libraries(z7-probe PRIVATE Qt6::Widgets)

  if(Z7_KWIN AND Z7_SPECTACLE AND Z7_MAGICK)
    set(Z7_SMOKE_ARGS --probe $<TARGET_FILE:z7-probe>
        --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/wayland_smoke")
    if(TARGET ei-input)
      list(APPEND Z7_SMOKE_ARGS --ei-input $<TARGET_FILE:ei-input>)
    endif()
    add_test(NAME wayland_smoke
      COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/wayland/test_smoke.py" ${Z7_SMOKE_ARGS})
    set_tests_properties(wayland_smoke PROPERTIES LABELS "wayland" TIMEOUT 120)
  else()
    message(STATUS "Wayland tests disabled (need kwin_wayland, spectacle and ImageMagick)")
  endif()
endif()

# Sanitizer builds: leaks of system libraries are suppressed (lsan.supp).
if(Z7_SANITIZE)
  get_property(z7_tests DIRECTORY PROPERTY TESTS)
  set_property(TEST ${z7_tests} APPEND PROPERTY ENVIRONMENT
    "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp:print_suppressions=0")
endif()
