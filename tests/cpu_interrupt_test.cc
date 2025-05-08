#include <gtest/gtest.h>

#include "cpu.h"
#include "testcommon/fake_mem.h"
#include "testcommon/prog_builder.h"

#define TEST_STACK_MAX_SIZE 4
#define TEST_PROG_MAX_SIZE  50
#define TEST_ISR_MAX_SIZE   50

#define TEST_MEM_BASE  CPU_IVT_ADDR
#define TEST_MEM_SIZE  2048
#define TEST_STACK_TOP (TEST_MEM_BASE + TEST_MEM_SIZE)

#define TEST_PROG_START (CPU_IVT_ADDR + CPU_IVT_SIZE)
#define TEST_ISR_START  (TEST_PROG_START + TEST_PROG_MAX_SIZE)

static_assert(TEST_PROG_START < (TEST_MEM_BASE + TEST_MEM_SIZE));
static_assert(TEST_PROG_START < TEST_ISR_START);
static_assert(TEST_PROG_START + TEST_PROG_MAX_SIZE <= TEST_ISR_START);
static_assert(TEST_ISR_START + TEST_ISR_MAX_SIZE <=
              (TEST_STACK_TOP - TEST_STACK_MAX_SIZE));

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

TEST_F(CPUInterruptTest, SoftwareInterrupt) {
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
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);

    // Now the CPU should see that the IRQ is raised and jump to the ISR.
    cpu_step(cpu);
    EXPECT_EQ(cpu->curr_int_line, CPU_IVT_FIRST_IRQ_ENTRY + irq_num);
    EXPECT_EQ(cpu->state, CPU_INT_PUSH_PC);
}

TEST_F(CPUInterruptTest, HALTHalts) {
    const auto instr = build_instr(CPU_OP_HALT).bytes;
    mem->write(TEST_PROG_START, instr.data(), instr.size());

    cpu->state = CPU_FETCH_DECODE_OPCODE;
    cpu->reg_pc = TEST_PROG_START;

    // Execute the HALT instruction.
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_HALTED);

    // Run a few steps to check that the CPU is still halted.
    for (int idx = 0; idx < 10; idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->state, CPU_HALTED);
    }
}

TEST_F(CPUInterruptTest, ExternalInterruptISR) {
    constexpr uint8_t irq_num = 1;

    // Write a HALT instruction.
    const auto instr_halt = build_instr(CPU_OP_HALT).bytes;
    mem->write(TEST_PROG_START, instr_halt.data(), instr_halt.size());
    const vm_addr_t addr_after_halt = TEST_PROG_START + instr_halt.size();

    // Write an IRET instruction to compose an ISR.
    const auto instr_iret = build_instr(CPU_OP_IRET).bytes;
    mem->write(TEST_ISR_START, instr_iret.data(), instr_iret.size());

    // Fill the IVT entry.
    const vm_addr_t isr_start = TEST_ISR_START;
    mem->write(CPU_IVT_ENTRY_ADDR(CPU_IVT_FIRST_IRQ_ENTRY + irq_num),
               &isr_start, CPU_IVT_ENTRY_SIZE);

    cpu->state = CPU_FETCH_DECODE_OPCODE;
    cpu->reg_pc = TEST_PROG_START;

    // Execute the HALT instruction.
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_HALTED);

    // Raise an IRQ and expect the CPU to jump to the ISR.
    cpu_raise_irq(cpu, irq_num);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_PUSH_PC);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_INT_JUMP);
    cpu_step(cpu);
    ASSERT_EQ(cpu->reg_pc, isr_start);

    // Execute the IRET instruction in the ISR.
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);
    cpu_step(cpu);
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);
    EXPECT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);
    EXPECT_EQ(cpu->reg_pc, addr_after_halt);
}
