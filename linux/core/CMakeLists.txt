# The 7-Zip core: all archive handlers, codecs and crypto, built from the
# upstream sources exactly as CPP/7zip/Bundles/Alone2/makefile.gcc does.
#
# It is an OBJECT library on purpose: handlers and codecs register themselves
# from static initializers, so every object file must reach the final link.

find_package(Threads REQUIRED)

z7_upstream_paths(Z7_CORE_SOURCES ${Z7_7ZZ_ARC_SOURCES} ${Z7_7ZZ_SYS_SOURCES})

add_library(z7_core OBJECT ${Z7_CORE_SOURCES})
target_compile_definitions(z7_core PUBLIC ${Z7_BASE_DEFINITIONS})
target_compile_options(z7_core PRIVATE ${Z7_UPSTREAM_WARNINGS} ${Z7_7ZZ_FLAGS})
target_link_libraries(z7_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# Upstream console 7zz built from the same lists: verifies that the core
# library matches the upstream build, and gives us a console tool.
set(Z7_7ZZ_REST ${Z7_7ZZ_SOURCES})
list(REMOVE_ITEM Z7_7ZZ_REST ${Z7_7ZZ_ARC_SOURCES} ${Z7_7ZZ_SYS_SOURCES})
z7_upstream_paths(Z7_7ZZ_REST_SOURCES ${Z7_7ZZ_REST})
z7_apply_source_flags(${Z7_7ZZ_SOURCE_FLAGS})

add_executable(7zz ${Z7_7ZZ_REST_SOURCES})
target_compile_options(7zz PRIVATE ${Z7_UPSTREAM_WARNINGS} ${Z7_7ZZ_FLAGS})
target_link_libraries(7zz PRIVATE z7_core)

# Oracle: upstream 7zz built by the upstream makefile itself.
if(Z7_BUILD_ORACLE)
  set(Z7_ORACLE_DIR "${CMAKE_BINARY_DIR}/oracle")
  set(Z7_ORACLE_7ZZ "${Z7_ORACLE_DIR}/7zz" CACHE INTERNAL "")
  include(ProcessorCount)
  ProcessorCount(Z7_NPROC)
  add_custom_target(oracle_7zz ALL
    COMMAND make -f makefile.gcc "O=${Z7_ORACLE_DIR}" "CFLAGS_WARN_WALL=-Wall -Wextra" -j${Z7_NPROC} --quiet
    WORKING_DIRECTORY "${Z7_ROOT}/CPP/7zip/Bundles/Alone2"
    BYPRODUCTS "${Z7_ORACLE_7ZZ}"
    COMMENT "Building upstream 7zz oracle with the upstream makefile"
    USES_TERMINAL)
endif()
