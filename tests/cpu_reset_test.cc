#include <gtest/gtest.h>

#include "cpu.h"
#include "testcommon/fake_mem.h"
#include "testcommon/prog_builder.h"

#define TEST_MEM_BASE 0x0000'0000
#define TEST_MEM_SIZE 500

#define TEST_PROG_START (TEST_MEM_BASE + 100)

class CPUResetTest : public testing::Test {
  public:
    CPUResetTest() {
        mem = new FakeMem(TEST_MEM_BASE, TEST_MEM_BASE + TEST_MEM_SIZE);
        cpu = cpu_new(&mem->mem_if);
    }
    ~CPUResetTest() {
        cpu_free(cpu);
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_F(CPUResetTest, InitStateIsReset) {
    EXPECT_EQ(cpu->state, CPU_RESET);
}

TEST_F(CPUResetTest, NoInitResetIsPossible) {
    cpu->state = CPU_FETCH_DECODE_OPCODE;
    cpu->reg_pc = TEST_PROG_START;

    auto prog = build_prog()
                    .instr(build_instr(CPU_OP_MOV_VR)
                               .reg_code(CPU_CODE_R0)
                               .imm32(0xDEADBEEF))
                    .bytes;
    mem->write(TEST_PROG_START, prog.data(), prog.size());

    cpu->gp_regs[0] = 0;

    do {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    } while (cpu->state != CPU_FETCH_DECODE_OPCODE);

    EXPECT_EQ(cpu->gp_regs[0], 0xDEADBEEF);
}

TEST_F(CPUResetTest, JumpsToResetISR) {
    vm_addr_t isr_entry_addr = CPU_IVT_ENTRY_ADDR(CPU_EXC_RESET);
    vm_addr_t reset_isr_addr = 0xDEADBEEF;
    mem->write(isr_entry_addr, &reset_isr_addr, sizeof(reset_isr_addr));

    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_JUMP);
    cpu_step(cpu);
    EXPECT_EQ(cpu->reg_pc, reset_isr_addr);
    EXPECT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);
}

TEST_F(CPUResetTest, TripleFaultCausesReset) {
    // A program that divides by zero is executed, triggering an exception. The
    // CPU jumps to the divide-by-zero ISR, but the ISR is the program itself.
    // This causes another divide-by-zero exception, and after the third
    // iteration, a triple fault occurs.

    const vm_addr_t ivt_reset_addr = CPU_IVT_ENTRY_ADDR(CPU_EXC_RESET);
    const vm_addr_t ivt_divzero_addr = CPU_IVT_ENTRY_ADDR(CPU_EXC_DIV_BY_ZERO);
    const vm_addr_t isr_reset_addr = TEST_PROG_START;
    const vm_addr_t isr_divzero_addr = TEST_PROG_START + 20;
    mem->write(ivt_reset_addr, &isr_reset_addr, sizeof(isr_reset_addr));
    mem->write(ivt_divzero_addr, &isr_divzero_addr, sizeof(isr_divzero_addr));

    const vm_addr_t stack_top = TEST_MEM_BASE + TEST_MEM_SIZE;
    const size_t stack_max_size = 4;
    const auto reset_prog =
        build_prog()
            .instr(build_instr(CPU_OP_MOV_VR)
                       .reg_code(CPU_CODE_SP)
                       .imm32(stack_top))
            .instr(build_instr(CPU_OP_JMPA_V32).imm32(isr_divzero_addr))
            .bytes;
    const size_t reset_prog_steps = 7;
    mem->write(isr_reset_addr, reset_prog.data(), reset_prog.size());

    const auto divzero_prog =
        build_prog()
            .instr(build_instr(CPU_OP_DIV_RV).reg_code(CPU_CODE_R0).imm32(0))
            .bytes;
    const size_t divzero_prog_steps = 4;
    mem->write(isr_divzero_addr, divzero_prog.data(), divzero_prog.size());

    // Ensure that reset and divide-by-zero programs, and the stack do not
    // overlap.
    ASSERT_LT(TEST_PROG_START + reset_prog.size(), isr_divzero_addr);
    ASSERT_LT(isr_divzero_addr + divzero_prog.size(),
              stack_top - stack_max_size);

    // Step through reset.
    ASSERT_EQ(cpu->state, CPU_RESET);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_JUMP);
    cpu_step(cpu);
    ASSERT_EQ(cpu->reg_pc, isr_reset_addr);
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);

    // Execute the reset program.
    for (size_t step = 0; step < reset_prog_steps; step++) {
        ASSERT_EQ(cpu->num_nested_exc, 0);
        cpu_step(cpu);
    }

    for (size_t reset_count = 0; reset_count < 3; reset_count++) {
        // fprintf(stderr, "test: reset count: %zu\n", reset_count);

        // Execute the divide-by-zero program / ISR.
        ASSERT_EQ(cpu->reg_pc, isr_divzero_addr);
        for (size_t step = 0; step < divzero_prog_steps; step++) {
            ASSERT_EQ(cpu->num_nested_exc, reset_count);
            cpu_step(cpu);
        }

        // Step through exception handling.
        if (reset_count < 2) {
            ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
            cpu_step(cpu);
            ASSERT_EQ(cpu->state, CPU_INT_PUSH_PC);
            cpu_step(cpu);
            ASSERT_EQ(cpu->state, CPU_INT_JUMP);
            cpu_step(cpu);
        }
    }
    ASSERT_EQ(cpu->state, CPU_TRIPLE_FAULT);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_RESET);
}
