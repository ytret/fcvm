include(FetchContent)

message("Fetching GoogleTest")
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/df1544bcee0c7ce35cd5ea0b3eb8cc81855a4140.zip
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

find_package(absl REQUIRED)

function(my_add_test name)
    set(compile_defs)
    foreach(arg ${ARGN})
        list(APPEND compile_defs "${ARGV${idx}}")
    endforeach()

    add_executable(${name} ${name}.cc)
    target_compile_options(${name} PRIVATE
        -Wall -Wextra -fdiagnostics-color=always
        -Wno-unknown-warning-option # sol.hpp
    )
    target_compile_definitions(${name} PRIVATE ${compile_defs})
    target_link_libraries(${name}
        fcvm GTest::gtest_main GTest::gmock_main absl::str_format
    )
    gtest_discover_tests(${name})
endfunction()

set(TEST_RNG_SEED 12345 CACHE STRING
    "Random number generator seed used by the tests.")
set(TEST_NUM_RANDOM_CASES 50 CACHE STRING
    "Number of randomly generated tests for each instruction.")

my_add_test(cpu_instr_data_test
    -DTEST_RNG_SEED=${TEST_RNG_SEED}
    -DTEST_NUM_RANDOM_CASES=${TEST_NUM_RANDOM_CASES}
)
my_add_test(cpu_instr_alu_test
    -DTEST_RNG_SEED=${TEST_RNG_SEED}
    -DTEST_NUM_RANDOM_CASES=${TEST_NUM_RANDOM_CASES}
)
my_add_test(cpu_instr_flow_test
    -DTEST_RNG_SEED=${TEST_RNG_SEED}
    -DTEST_NUM_RANDOM_CASES=${TEST_NUM_RANDOM_CASES}
)
my_add_test(cpu_instr_stack_test
    -DTEST_RNG_SEED=${TEST_RNG_SEED}
    -DTEST_NUM_RANDOM_CASES=${TEST_NUM_RANDOM_CASES}
)

my_add_test(cpu_exception_test)
my_add_test(cpu_reset_test)

my_add_test(intctl_test)
