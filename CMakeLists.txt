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
if(CMAKE_VERSION VERSION_GREATER 3.29)
    # CMP0167: CMake 3.30 and above prefer to not provide the `FindBoost` module
    # so that `find_package(Boost)` calls, without the `CONFIG` or `NO_MODULE`
    # options, find the upstream `BoostConfig.cmake` directly. This policy
    # provides compatibility for projects that have not been ported to use the
    # upstream Boost package.
    cmake_policy(SET CMP0167 NEW)
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
    src/cpu/cpu.c
    src/cpu/cpu_exec.c
    src/cpu/cpu_instr_descs.c
    src/cpu/cpu_stack.c
    src/intctl.c
    src/memctl.c
    src/vm.c
)
target_compile_options(fcvm PRIVATE
    -g -Wall -Wextra -Wmissing-prototypes
    -fdiagnostics-color=always
)
target_include_directories(fcvm PUBLIC inc src)


set(FCVM_ASM_DIR ${PROJECT_SOURCE_DIR}/tools/asm-fennel)
set(FCVM_ASM ${FCVM_ASM_DIR}/main.fnl)


add_subdirectory(tests)
add_subdirectory(docs)
add_subdirectory(examples)
