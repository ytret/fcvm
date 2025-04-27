include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/df1544bcee0c7ce35cd5ea0b3eb8cc81855a4140.zip
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

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
        fcvm GTest::gtest_main GTest::gmock_main
    )
    gtest_discover_tests(${name})
endfunction()

my_add_test(vm_state_test)
my_add_test(cpu_data_test)
my_add_test(cpu_alu_test)
my_add_test(cpu_control_flow_test)
