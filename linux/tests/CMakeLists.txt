find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME upstream_sources_current
  COMMAND Python3::Interpreter -I "${Z7_ROOT}/linux/tools/upstream_sources.py" --check)

if(Z7_BUILD_ORACLE)
  add_test(NAME core_roundtrip
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/core/test_roundtrip.py"
            --ours $<TARGET_FILE:7zz> --oracle "${Z7_ORACLE_7ZZ}")
  set_tests_properties(core_roundtrip PROPERTIES LABELS "core" TIMEOUT 300)
endif()
