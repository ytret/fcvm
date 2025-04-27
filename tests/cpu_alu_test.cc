#include <gtest/gtest.h>

extern "C" {
#include "cpu.h"
#include "ram.h"
#include "vm.h"
}

#define RAM_SIZE 128

class CPUALUTest : public testing::Test {
  protected:
    CPUALUTest() {
        _vm = vm_new();
        _ram1 = ram_init(RAM_SIZE);
        ram_clear(_ram1);

        mmio_t ram1_mmio = {
            .loaded = true,
            .type = MMIO_RAM,
            .base = 0,
            .size = RAM_SIZE,
            .ctx = _ram1,

            .pf_read_u8 = ram_read_u8,
            .pf_write_u8 = ram_write_u8,
            .pf_read_u32 = ram_read_u32,
            .pf_write_u32 = ram_write_u32,
            .pf_deinit = ram_free,
            .pf_state_size = ram_state_size,
            .pf_state_save = ram_state_save,
        };
        vm_map_device(_vm, &ram1_mmio);
    }
    ~CPUALUTest() {
        vm_free(_vm);
        free(_ram1);
    }

    vm_state_t *_vm;
    ram_t *_ram1;
};

TEST_F(CPUALUTest, ResetClearsRegisters) {
    memset(_vm->regs_gp, 0xFFFFFFFF, sizeof(_vm->regs_gp));
    _vm->reg_pc = 0xFFFFFFFF;
    _vm->reg_sp = 0xFFFFFFFF;
    cpu_reset(_vm);

    for (int i = 0; i < VM_NUM_GP_REGS; i++) {
        EXPECT_EQ(_vm->regs_gp[i], 0);
    }
    // TODO: test PC value (reset handler).
    EXPECT_EQ(_vm->reg_sp, 0);
}

TEST_F(CPUALUTest, Instr_ADD_RR) {
    constexpr uint32_t val1 = 0xDEADBEEF;
    constexpr uint32_t val2 = 0xCAFEBABE;
    constexpr uint32_t sum = val1 + val2; // truncated from 0x1_A9AC_79AD
    constexpr uint32_t flags = CPU_FLAG_SIGN | CPU_FLAG_CARRY;
    // SIGN - bit 31 of sum is set
    // CARRY - higher dword of 64-bit sum is non-zero

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_ADD_RR; // R0 = R0 + R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], sum);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_SUB_RR) {
    constexpr uint32_t val1 = 0xCAFEBABE;
    constexpr uint32_t val2 = 0xDEADBEEF;
    constexpr int32_t diff = val1 - val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_SUB_RR; // R0 = R0 - R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], (uint32_t)diff);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_MUL_RR) {
    constexpr uint32_t val1 = 0xCAFEBABE;
    constexpr uint32_t val2 = 0xDEADBEEF;
    constexpr uint32_t prod = val1 * val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN | CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_MUL_RR; // R0 = R0 * R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], prod);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_DIV_RR) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = -2;
    constexpr uint32_t quot = val1 / val2;
    constexpr uint32_t flags = CPU_FLAG_ZERO;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_DIV_RR; // R0 = R0 / R1 (unsigned)
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], quot);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_IDIV_RR) {
    constexpr int32_t val1 = 9;
    constexpr int32_t val2 = -2;
    constexpr int32_t quot = val1 / val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_IDIV_RR; // R0 = R0 / R1 (signed)
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], (uint32_t)quot);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_AND_RR) {
    constexpr uint32_t val1 = (1 << 10) | (1 << 1);
    constexpr uint32_t val2 = (1 << 9) | (1 << 0);
    constexpr uint32_t res = val1 & val2;
    constexpr uint32_t flags = CPU_FLAG_ZERO;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_AND_RR; // R0 = R0 & R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_OR_RR) {
    constexpr uint32_t val1 = (1 << 31) | (1 << 1);
    constexpr uint32_t val2 = (1 << 9) | (1 << 0);
    constexpr uint32_t res = val1 | val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_OR_RR; // R0 = R0 | R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_XOR_RR) {
    constexpr uint32_t val1 = (1 << 31) | (1 << 9);
    constexpr uint32_t val2 = (1 << 9) | (1 << 0);
    constexpr uint32_t res = val1 ^ val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_XOR_RR; // R0 = R0 ^ R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_NOT_R) {
    constexpr uint32_t val = 0x00C0FFEE;
    constexpr uint32_t res = ~val;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_NOT_R; // R0 = ~R0
    _ram1->buf[1] = CPU_CODE_R0;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_SHL_RR) {
    constexpr uint32_t val1 = 1 | (1 << 30);
    constexpr uint32_t val2 = 2;
    constexpr uint32_t res = val1 << val2;
    constexpr uint32_t flags = CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_SHL_RR; // R0 = R0 << R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_SHR_RR) {
    constexpr uint32_t val1 = 0b110;
    constexpr uint32_t val2 = 2;
    constexpr uint32_t res = val1 >> val2;
    constexpr uint32_t flags = CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_SHR_RR; // R0 = R0 >> R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], res);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_CMP_RR) {
    constexpr uint32_t val1 = (1 << 31);
    constexpr uint32_t val2 = (1 << 31);
    constexpr uint32_t flags = CPU_FLAG_ZERO | CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR; // R0 - R1, set flags
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], val1);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_CMP_RR_EQ) {
    constexpr uint32_t val1 = -123;
    constexpr uint32_t val2 = -123;
    constexpr uint32_t flags = CPU_FLAG_ZERO | CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_CMP_RR_NE) {
    constexpr uint32_t val1 = -123;
    constexpr uint32_t val2 = -122;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_CMP_RR_GT) {
    constexpr uint32_t val1 = -123;
    constexpr uint32_t val2 = -124;
    constexpr uint32_t flags = CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_CMP_RR_LT) {
    constexpr uint32_t val1 = -124;
    constexpr uint32_t val2 = -123;
    constexpr uint32_t flags = CPU_FLAG_SIGN;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUALUTest, Instr_TST_RR) {
    constexpr uint32_t val1 = (1 << 31);
    constexpr uint32_t val2 = (1 << 30);
    constexpr uint32_t flags = CPU_FLAG_ZERO;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_TST_RR; // R1 & R0, set flags
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], val1);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}
