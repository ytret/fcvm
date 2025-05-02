#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

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
    } while (cpu->state != CPU_EXECUTED_OK);

    EXPECT_EQ(cpu->gp_regs[0], 0xDEADBEEF);
}
