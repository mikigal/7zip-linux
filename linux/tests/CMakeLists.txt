find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME upstream_sources_current
  COMMAND Python3::Interpreter -I "${Z7_ROOT}/linux/tools/upstream_sources.py" --check)

if(Z7_BUILD_ORACLE)
  add_test(NAME core_roundtrip
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/core/test_roundtrip.py"
            --ours $<TARGET_FILE:7zz> --oracle "${Z7_ORACLE_7ZZ}")
  set_tests_properties(core_roundtrip PROPERTIES LABELS "core" TIMEOUT 300)
endif()

# Wayland scenario tests run in a nested headless KWin (label "wayland").
find_package(Qt6 COMPONENTS Widgets)
find_program(Z7_KWIN kwin_wayland)
find_program(Z7_SPECTACLE spectacle)
find_program(Z7_MAGICK magick)

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
