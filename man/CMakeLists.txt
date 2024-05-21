option(INSTALL_MANPAGE "install manpage" ON)

find_program(GZIP_EXECUTABLE gzip)

if (INSTALL_MANPAGE)
  if (NOT GZIP_EXECUTABLE)
        message(FATAL_ERROR "gzip not found: can not install man page")
  endif()

  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hwmondump.1.gz"
        COMMAND ${GZIP_EXECUTABLE}
        ARGS -9 --force --keep --to-stdout <"${CMAKE_CURRENT_SOURCE_DIR}/hwmondump.1" >"${CMAKE_CURRENT_BINARY_DIR}/hwmondump.1.gz"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/hwmondump.1"
        COMMENT "Compressing man pages"
  )

  add_custom_target(man ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/hwmondump.1.gz")

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/hwmondump.1.gz"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
  )
endif()
