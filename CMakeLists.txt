cmake_minimum_required(VERSION 3.18)
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


##################
## Library code ##
##################
add_library(fcvm STATIC
    src/cpu.c
    src/mem.c
    src/ram.c
    src/vm.c
)
target_compile_options(fcvm PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
)
target_include_directories(fcvm PUBLIC inc)


##################
## Main program ##
##################
add_executable(fcvm-main
    src/main.c
)
target_compile_options(fcvm-main PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
)
target_link_libraries(fcvm-main fcvm)


###########
## Tests ##
###########
add_subdirectory(tests)


##########
## Docs ##
##########
#add_subdirectory(docs)
