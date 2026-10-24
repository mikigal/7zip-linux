# The language files of the 7-Zip release (README.md): the build puts them
# in Lang/ next to 7zFM and 7zG, where the programs look for them.

file(GLOB Z7_LANG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.txt"
     "${CMAKE_CURRENT_SOURCE_DIR}/*.ttt")
list(REMOVE_ITEM Z7_LANG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
set(Z7_LANG_STAMP "${CMAKE_CURRENT_BINARY_DIR}/lang.stamp")
add_custom_command(OUTPUT "${Z7_LANG_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:7zFM>/Lang" "$<TARGET_FILE_DIR:7zG>/Lang"
  COMMAND "${CMAKE_COMMAND}" -E copy ${Z7_LANG_FILES} "$<TARGET_FILE_DIR:7zFM>/Lang"
  COMMAND "${CMAKE_COMMAND}" -E copy ${Z7_LANG_FILES} "$<TARGET_FILE_DIR:7zG>/Lang"
  COMMAND "${CMAKE_COMMAND}" -E touch "${Z7_LANG_STAMP}"
  DEPENDS ${Z7_LANG_FILES}
  COMMENT "Copying the 7-Zip language files")
add_custom_target(z7_lang ALL DEPENDS "${Z7_LANG_STAMP}")
