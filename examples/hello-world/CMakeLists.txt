add_executable(ex_hello_world
    main.c
)
target_compile_options(ex_hello_world PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
    -fdiagnostics-color=always
    -fanalyzer
)
target_include_directories(ex_hello_world PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ex_hello_world ex_common)
