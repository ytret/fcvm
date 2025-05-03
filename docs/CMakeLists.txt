find_package(Doxygen)

if(Doxygen_FOUND)
    set(DOXYGEN_INDEX_FILE ${CMAKE_CURRENT_SOURCE_DIR}/output/html/index.html)

    configure_file(Doxyfile.in Doxyfile)
    set(DOXYFILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    add_custom_command(
        OUTPUT ${DOXYGEN_INDEX_FILE}
        DEPENDS fcvm ${CMAKE_SOURCE_DIR}/README.md
        COMMAND ${DOXYGEN_EXECUTABLE} -q ${DOXYFILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        MAIN_DEPENDENCY Doxyfile
        COMMENT "Generating documentation"
    )
    add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})
endif()
