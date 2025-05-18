add_library(ex_common STATIC
    src/file_rom.c
    src/print_dev.c
)
target_compile_options(ex_common PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
    -fdiagnostics-color=always
    -fanalyzer
)
target_include_directories(ex_common PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
target_link_libraries(ex_common fcvm)
