#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_BAD_MEM 0x1000'0000

class CPUExceptionTest : public testing::Test {
  protected:
    CPUExceptionTest() {
    }
    ~CPUExceptionTest() {
        cpu_free(cpu);
        delete mem;
    }

    void init_cpu(bool map_ivt) {
        size_t ivt_size = CPU_IVT_ENTRY_SIZE * CPU_IVT_NUM_ENTRIES;

        mem_base = map_ivt ? 0x0000'0000 : 0x0000'1000;
        mem_size = mem_base + ivt_size + 100;
        prog_start = mem_base + ivt_size;
        stack_top = mem_base + mem_size;

        mem = new FakeMem(mem_base, mem_size);

        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = prog_start;
        cpu->reg_sp = stack_top;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;

    vm_addr_t mem_base;
    size_t mem_size;
    vm_addr_t prog_start;
    vm_addr_t stack_top;
};

TEST_F(CPUExceptionTest, BadMem) {
    init_cpu(true);

    vm_addr_t ivt_entry_addr =
        CPU_IVT_ADDR + CPU_IVT_ENTRY_SIZE * VM_ERR_BAD_MEM;
    vm_addr_t isr_addr = 0xDEADBEEF;
    mem->write(ivt_entry_addr, &isr_addr, sizeof(isr_addr));

    auto instr_bytes = build_instr(CPU_OP_STR_RV0)
                           .reg_code(CPU_CODE_R0)
                           .imm32(TEST_BAD_MEM)
                           .bytes;
    mem->write(prog_start, instr_bytes.data(), instr_bytes.size());
    uint32_t bad_instr_addr = prog_start;

    for (int step = 0; step < 3; step++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }

    // Try to write to bad memory.
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);

    // Fetch ISR address.
    ASSERT_EQ(cpu->num_nested_exc, 1);
    ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
    cpu_step(cpu);

    // Push PC.
    ASSERT_EQ(cpu->state, CPU_INT_PUSH_PC);
    cpu_step(cpu);
    uint32_t pushed_val;
    mem->read(stack_top - 4, &pushed_val, 4);
    EXPECT_EQ(pushed_val, bad_instr_addr);

    // Jump to the ISR.
    ASSERT_EQ(cpu->state, CPU_INT_JUMP);
    cpu_step(cpu);
    EXPECT_EQ(cpu->reg_pc, isr_addr);
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);
}
