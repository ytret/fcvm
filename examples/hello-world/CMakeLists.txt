add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hello-world.bin
    COMMAND ${FCVM_ASM}
            ${CMAKE_CURRENT_LIST_DIR}/hello-world.asm
            ${CMAKE_CURRENT_BINARY_DIR}/hello-world.bin
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/hello-world.asm
    WORKING_DIRECTORY ${FCVM_DIR}
    COMMENT "Assembling hello-world.asm"
    VERBATIM
)
add_custom_target(hello_world_bin
    DEPENDS hello-world.bin
)

add_executable(ex_hello_world
    main.c
)
target_compile_options(ex_hello_world PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
    -fdiagnostics-color=always
)
target_compile_definitions(ex_hello_world PRIVATE
    -DEX_HELLO_WORLD_GUEST="${CMAKE_CURRENT_BINARY_DIR}/hello-world.bin"
)
target_include_directories(ex_hello_world PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ex_hello_world ex_common)
add_dependencies(ex_hello_world hello_world_bin)
