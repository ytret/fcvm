cmake_minimum_required(VERSION 3.18)
if(CMAKE_VERSION VERSION_GREATER 3.19)
    # CMP0115: Source file extensions must be explicit.
    # 3.20+
    cmake_policy(SET CMP0115 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER 3.23)
    # CMP0135: When using the URL download method with the ExternalProject_Add()
    # or FetchContent_Declare() commands, set the timestamps of extracted
    # contents to the time of extraction.
    # 3.24+
    cmake_policy(SET CMP0135 NEW)
endif()

project(fcvm C CXX)
enable_testing()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


##################
## Library code ##
##################
add_library(fcvm STATIC
    src/busctl.c
    src/cpu.c
    src/cpu_instr_descs.c
    src/intctl.c
    src/memctl.c
    src/vm.c
)
target_compile_options(fcvm PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
    -fdiagnostics-color=always
)
target_include_directories(fcvm PUBLIC inc)


###########
## Tests ##
###########
add_subdirectory(tests)


##########
## Docs ##
##########
add_subdirectory(docs)
