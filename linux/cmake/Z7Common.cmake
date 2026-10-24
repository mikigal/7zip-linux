# Common compiler settings and helpers for the Linux port build.

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Same base definitions as CFLAGS_BASE in CPP/7zip/7zip_gcc.mak.
set(Z7_BASE_DEFINITIONS _REENTRANT _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)

# Upstream code: upstream warning level, but never -Werror, because newer
# compilers report false positives (for example -Warray-bounds in GCC 16).
set(Z7_UPSTREAM_WARNINGS -Wall -Wextra)

# Linux port code: stricter, and errors when Z7_WERROR is on.
set(Z7_PORT_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
if(Z7_WERROR)
  list(APPEND Z7_PORT_WARNINGS -Werror)
endif()

if(Z7_SANITIZE)
  add_compile_options(-fsanitize=${Z7_SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${Z7_SANITIZE})
endif()

# z7_upstream_paths(<out-var> <relative paths...>)
# Converts repository-relative paths from UpstreamSources.cmake to absolute ones.
function(z7_upstream_paths out)
  set(result)
  foreach(p IN LISTS ARGN)
    list(APPEND result "${Z7_ROOT}/${p}")
  endforeach()
  set(${out} ${result} PARENT_SCOPE)
endfunction()

# z7_apply_source_flags(<list of "path|flags" entries>)
# Applies per-object flags that upstream rules pass in addition to CXXFLAGS.
function(z7_apply_source_flags)
  foreach(entry IN LISTS ARGN)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 path)
    list(GET parts 1 flags)
    separate_arguments(flags UNIX_COMMAND "${flags}")
    set_property(SOURCE "${Z7_ROOT}/${path}" APPEND PROPERTY COMPILE_OPTIONS ${flags})
  endforeach()
endfunction()

# z7_port_target(<target>): settings for code written for the Linux port.
function(z7_port_target target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_options(${target} PRIVATE ${Z7_PORT_WARNINGS})
endfunction()
