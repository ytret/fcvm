find_program(CARGO cargo REQUIRED)

set(FCVM_ASM_BUILD_TYPE "Debug" CACHE STRING
    "tools/asm-rust build type: Debug or Release.")

# Target dir for the cargo's layout.
set(FCVM_ASM_CARGO_DIR ${CMAKE_CURRENT_BINARY_DIR}/cargo)

# Final published binary, used by the examples.
set(FCVM_ASM_BIN ${FCVM_TOOLS_DIR}/asm-rust)

if(FCVM_ASM_BUILD_TYPE STREQUAL Debug)
    set(FCVM_ASM_CARGO_BUILD_OPT "")
    set(FCVM_ASM_CARGO_PROFILE "debug")
elseif(FCVM_ASM_BUILD_TYPE STREQUAL Release)
    set(FCVM_ASM_CARGO_BUILD_OPT "--release")
    set(FCVM_ASM_CARGO_PROFILE "release")
else()
    message(FATAL_ERROR "Unrecognized value of FCVM_ASM_BUILD_TYPE: ${FCVM_ASM_BUILD_TYPE}")
endif()

# Re-run cargo when any source file changes. CONFIGURE_DEPENDS makes CMake
# re-glob when the build system is refreshed, so new files are picked up
# without a manual reconfigure.
file(GLOB FCVM_ASM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*.rs
)

add_custom_command(
    OUTPUT ${FCVM_ASM_BIN}
    COMMAND ${CMAKE_COMMAND} -E env
            "CARGO_TARGET_DIR=${FCVM_ASM_CARGO_DIR}"
            ${CARGO} build ${FCVM_ASM_CARGO_BUILD_OPT}
            --manifest-path ${CMAKE_CURRENT_LIST_DIR}/Cargo.toml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FCVM_ASM_CARGO_DIR}/${FCVM_ASM_CARGO_PROFILE}/asm-rust
            ${FCVM_ASM_BIN}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/Cargo.toml
            ${FCVM_ASM_SOURCES}
    COMMENT "Building asm-rust assembler"
    VERBATIM
    USES_TERMINAL
)
add_custom_target(asm_rust DEPENDS ${FCVM_ASM_BIN})
