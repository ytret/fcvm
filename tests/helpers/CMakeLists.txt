add_executable(cpu_snapshot_step
    cpu_snapshot_step.cc
)
target_compile_options(cpu_snapshot_step PRIVATE
    -Wall -Wextra -fdiagnostics-color=always
)
target_link_libraries(cpu_snapshot_step
    fcvm testcommon GTest::gtest_main
)
