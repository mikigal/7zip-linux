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

# Unit tests (label "unit"): QtTest programs, run with a private HOME,
# XDG_CONFIG_HOME and no session bus, so that they never see the user's
# settings or desktop.
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
  # no session bus: the shell code would talk to the user's desktop
  set_tests_properties(unit_${name} PROPERTIES LABELS "unit" TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${Z7_UNIT_HOME};XDG_CONFIG_HOME=${Z7_UNIT_HOME}/config;XDG_DATA_HOME=${Z7_UNIT_HOME}/data;XDG_CACHE_HOME=${Z7_UNIT_HOME}/cache;XDG_STATE_HOME=${Z7_UNIT_HOME}/state;DBUS_SESSION_BUS_ADDRESS=unix:path=${Z7_UNIT_HOME}/no-bus"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")
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
  z7_unit_test(packaging unit/test_packaging.cpp)
else()
  message(STATUS "Unit tests disabled (need Qt6 Test)")
endif()

# The script engine of the GUI test drivers (gui/DriverCore.cpp).
if(TARGET z7_gui)
  add_library(z7_driver_core STATIC gui/DriverCore.cpp)
  z7_port_target(z7_driver_core)
  set_target_properties(z7_driver_core PROPERTIES AUTOMOC ON)
  target_compile_options(z7_driver_core PRIVATE -Wno-non-virtual-dtor)
  target_include_directories(z7_driver_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/gui")
  target_link_libraries(z7_driver_core PUBLIC z7_gui z7_theme)
endif()

# z7-gui-driver: 7zG with the dialogs answered by a script (gui/gui_driver.cpp),
# for the end-to-end tests of 7zG.
if(TARGET 7zG)
  add_executable(z7-gui-driver gui/gui_driver.cpp)
  z7_port_target(z7-gui-driver)
  # Upstream COM interfaces have no virtual destructors by design.
  target_compile_options(z7-gui-driver PRIVATE -Wno-non-virtual-dtor)
  z7_link_ui(z7-gui-driver GUI)
  target_link_libraries(z7-gui-driver PRIVATE z7_7zg_main z7_driver_core z7_gui z7_theme)

  if(Z7_BUILD_ORACLE)
    add_test(NAME gui_7zg
      COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/gui/test_7zg.py"
              --driver $<TARGET_FILE:z7-gui-driver> --oracle "${Z7_ORACLE_7ZZ}"
              --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/gui_7zg")
    set_tests_properties(gui_7zg PROPERTIES LABELS "gui" TIMEOUT 600)
  endif()
endif()

# z7-fm-driver: 7zFM with its main window and dialogs worked by a script
# (gui/fm_driver.cpp), for the end-to-end tests of 7zFM.
if(TARGET 7zFM)
  add_executable(z7-fm-driver gui/fm_driver.cpp)
  z7_port_target(z7-fm-driver)
  target_compile_options(z7-fm-driver PRIVATE -Wno-non-virtual-dtor)
  z7_link_ui(z7-fm-driver FM)
  target_link_libraries(z7-fm-driver PRIVATE z7_driver_core
    "$<LINK_GROUP:RESCAN,z7_fm,z7_fm_linux>" z7_gui z7_theme Qt6::Test)

  if(Z7_BUILD_ORACLE)
    add_test(NAME gui_7zfm
      COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/gui/test_7zfm.py"
              --driver $<TARGET_FILE:z7-fm-driver> --oracle "${Z7_ORACLE_7ZZ}"
              --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/gui_7zfm")
    set_tests_properties(gui_7zfm PROPERTIES LABELS "gui" TIMEOUT 600)
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

    if(TARGET z7-gui-driver AND Z7_BUILD_ORACLE)
      set(Z7_WL7ZG_ARGS --driver $<TARGET_FILE:z7-gui-driver> --oracle "${Z7_ORACLE_7ZZ}"
          --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/wayland_7zg")
      if(TARGET ei-input)
        list(APPEND Z7_WL7ZG_ARGS --ei-input $<TARGET_FILE:ei-input>)
      endif()
      add_test(NAME wayland_7zg
        COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/wayland/test_7zg_wayland.py" ${Z7_WL7ZG_ARGS})
      set_tests_properties(wayland_7zg PROPERTIES LABELS "wayland" TIMEOUT 300)
    endif()

    if(TARGET z7-fm-driver)
      set(Z7_WL7ZFM_ARGS --driver $<TARGET_FILE:z7-fm-driver>
          --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/wayland_7zfm")
      if(TARGET ei-input)
        list(APPEND Z7_WL7ZFM_ARGS --ei-input $<TARGET_FILE:ei-input>)
      endif()
      add_test(NAME wayland_7zfm
        COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/wayland/test_7zfm_wayland.py" ${Z7_WL7ZFM_ARGS})
      set_tests_properties(wayland_7zfm PROPERTIES LABELS "wayland" TIMEOUT 300)
      # drag and drop with other clients needs the pointer of ei-input
      if(TARGET ei-input)
        add_test(NAME wayland_dnd
          COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/wayland/test_dnd_wayland.py"
                  --driver $<TARGET_FILE:z7-fm-driver> --probe $<TARGET_FILE:z7-probe>
                  --ei-input $<TARGET_FILE:ei-input> --oracle "${Z7_ORACLE_7ZZ}"
                  --artifacts "${CMAKE_BINARY_DIR}/test-artifacts/wayland_dnd")
        set_tests_properties(wayland_dnd PROPERTIES LABELS "wayland" TIMEOUT 400)
      endif()
    endif()
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

# The desktop files of linux/packaging (label "unit").
find_program(Z7_DESKTOP_FILE_VALIDATE desktop-file-validate)
if(Z7_DESKTOP_FILE_VALIDATE)
  file(GLOB Z7_DESKTOP_FILES "${Z7_ROOT}/linux/packaging/*.desktop")
  add_test(NAME desktop_files COMMAND "${Z7_DESKTOP_FILE_VALIDATE}" ${Z7_DESKTOP_FILES})
  set_tests_properties(desktop_files PROPERTIES LABELS "unit" TIMEOUT 60)
else()
  message(STATUS "desktop_files test disabled (need desktop-file-validate)")
endif()
