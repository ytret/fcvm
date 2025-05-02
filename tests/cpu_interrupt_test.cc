#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_PROG_MAX_SIZE  50
#define TEST_STACK_MAX_SIZE 4

#define TEST_MEM_BASE   CPU_IVT_ADDR
#define TEST_MEM_SIZE   (CPU_IVT_SIZE + TEST_PROG_MAX_SIZE + TEST_STACK_MAX_SIZE)
#define TEST_PROG_START (CPU_IVT_ADDR + CPU_IVT_SIZE)
#define TEST_STACK_TOP  (TEST_MEM_BASE + TEST_MEM_SIZE)

class CPUInterruptTest : public testing::Test {
  protected:
    CPUInterruptTest() {
        mem = new FakeMem(TEST_MEM_BASE, TEST_MEM_BASE + TEST_MEM_SIZE);
        cpu = cpu_new(&mem->mem_if);
        cpu->reg_sp = TEST_STACK_TOP;
    }

    ~CPUInterruptTest() {
        cpu_free(cpu);
        delete mem;
    }

    cpu_ctx_t *cpu;
    FakeMem *mem;
};

TEST_F(CPUInterruptTest, HandleIRQ) {
    constexpr uint8_t irq_num = 1;
    auto instr = build_instr(CPU_OP_INT_V8).imm8(irq_num).bytes;
    mem->write(TEST_PROG_START, instr.data(), instr.size());

    cpu->state = CPU_FETCH_DECODE_OPCODE;
    cpu->reg_pc = TEST_PROG_START;

    // Execute the INT instruction.
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPERANDS);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_EXECUTED_OK);

    // Now the CPU should see that the IRQ is raised and jump to the ISR.
    cpu_step(cpu);
    EXPECT_EQ(cpu->curr_int_line, CPU_IVT_FIRST_IRQ_ENTRY + irq_num);
    EXPECT_EQ(cpu->state, CPU_INT_PUSH_PC);
}
