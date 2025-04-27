#include <gtest/gtest.h>

extern "C" {
#include "cpu.h"
#include "ram.h"
#include "vm.h"
}

#define RAM_SIZE 128

class CPUTest : public testing::Test {
  protected:
    CPUTest() {
        _vm = vm_new();
        _ram1 = ram_init(RAM_SIZE);

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
    ~CPUTest() {
        vm_free(_vm);
        free(_ram1);
    }

    vm_state_t *_vm;
    ram_t *_ram1;
};

TEST_F(CPUTest, ResetClearsRegisters) {
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

TEST_F(CPUTest, Instr_MOV_RR) {
    _vm->regs_gp[0] = 0xDEADBEEF;
    _vm->regs_gp[1] = 0xCAFEBABE;

    _ram1->buf[0] = CPU_OP_MOV_RR;
    _ram1->buf[1] = (CPU_CODE_R1 << 4) | CPU_CODE_R0; // R1 -> R0
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[1], 0xCAFEBABE);
    EXPECT_EQ(_vm->regs_gp[0], 0xCAFEBABE);
}

TEST_F(CPUTest, Instr_MOV_VR) {
    _vm->regs_gp[0] = 0xDEADBEEF;
    uint32_t val = 0xCAFEBABE;

    uint8_t *ram_u8 = _ram1->buf;
    ram_u8[0] = CPU_OP_MOV_VR;
    ram_u8[1] = CPU_CODE_R0;     // R0 <- ..
    memcpy(&ram_u8[2], &val, 4); // .. <- val
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], 0xCAFEBABE);
}

TEST_F(CPUTest, Instr_STR_RI0) {
    constexpr uint32_t mem_addr = 12;
    static_assert(mem_addr + 4 <= RAM_SIZE);

    memset(&_ram1->buf[mem_addr], 0xFF, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;
    _vm->regs_gp[1] = mem_addr;

    _ram1->buf[0] = CPU_OP_STR_RI0;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1; // R0 -> mem[R1]
    cpu_step(_vm);

    uint32_t written_bytes;
    memcpy(&written_bytes, &_ram1->buf[mem_addr], 4);
    EXPECT_EQ(written_bytes, 0xDEADBEEF);
}

TEST_F(CPUTest, Instr_STR_RV0) {
    constexpr uint32_t mem_addr = 12;
    static_assert(mem_addr + 4 <= RAM_SIZE);

    memset(&_ram1->buf[mem_addr], 0xFF, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;

    _ram1->buf[0] = CPU_OP_STR_RV0;
    _ram1->buf[1] = CPU_CODE_R0;          // R0 -> ..
    memcpy(&_ram1->buf[2], &mem_addr, 4); // .. -> mem[mem_addr]
    cpu_step(_vm);

    uint32_t written_bytes;
    memcpy(&written_bytes, &_ram1->buf[mem_addr], 4);
    EXPECT_EQ(written_bytes, 0xDEADBEEF);
}

TEST_F(CPUTest, Instr_STR_RI8) {
    constexpr uint32_t mem_addr = 12;
    constexpr int8_t imm_off8 = -2;
    static_assert(mem_addr + imm_off8 + 4 <= RAM_SIZE);

    memset(&_ram1->buf[mem_addr + imm_off8], 0xFF, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;
    _vm->regs_gp[1] = mem_addr;

    _ram1->buf[0] = CPU_OP_STR_RI8;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1; // R0 -> mem[R1 + ..
    _ram1->buf[2] = (uint8_t)imm_off8;                // .. + imm8]
    cpu_step(_vm);

    uint32_t written_bytes;
    memcpy(&written_bytes, &_ram1->buf[mem_addr + imm_off8], 4);
    EXPECT_EQ(written_bytes, 0xDEADBEEF);
}

TEST_F(CPUTest, Instr_STR_RI32) {
    constexpr uint32_t mem_addr = 12;
    constexpr int32_t imm_off32 = -2;
    static_assert(mem_addr + imm_off32 + 4 <= RAM_SIZE);

    memset(&_ram1->buf[mem_addr + imm_off32], 0xFF, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;
    _vm->regs_gp[1] = mem_addr;

    _ram1->buf[0] = CPU_OP_STR_RI32;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1; // R0 -> mem[R1 + ..
    memcpy(&_ram1->buf[2], &imm_off32, 4);            // .. + imm32]
    cpu_step(_vm);

    uint32_t written_bytes;
    memcpy(&written_bytes, &_ram1->buf[mem_addr + imm_off32], 4);
    EXPECT_EQ(written_bytes, 0xDEADBEEF);
}

TEST_F(CPUTest, Instr_STR_RIR) {
    constexpr uint32_t mem_addr = 12;
    constexpr int32_t off32 = -2;
    static_assert(mem_addr + off32 + 4 <= RAM_SIZE);

    memset(&_ram1->buf[mem_addr + off32], 0xFF, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;
    _vm->regs_gp[1] = mem_addr;
    _vm->regs_gp[2] = off32;

    _ram1->buf[0] = CPU_OP_STR_RIR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1; // R0 -> mem[R1 + ..
    _ram1->buf[2] = CPU_CODE_R2;                      // .. + R2]
    cpu_step(_vm);

    uint32_t written_bytes;
    memcpy(&written_bytes, &_ram1->buf[mem_addr + off32], 4);
    EXPECT_EQ(written_bytes, 0xDEADBEEF);
}

TEST_F(CPUTest, Instr_LDR_RV0) {
    constexpr uint32_t mem_addr = 12;
    constexpr uint32_t mem_dword = 0xCAFEBABE;
    static_assert(mem_addr + 4 <= RAM_SIZE);

    memcpy(&_ram1->buf[mem_addr], &mem_dword, 4);
    _vm->regs_gp[0] = 0xDEADBEEF;

    _ram1->buf[0] = CPU_OP_LDR_RV0; // mem[imm32] -> R0
    _ram1->buf[1] = CPU_CODE_R0;
    memcpy(&_ram1->buf[2], &mem_addr, 4);
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], mem_dword);
}

TEST_F(CPUTest, Instr_LDR_RI0) {
    constexpr uint32_t mem_addr = 12;
    constexpr uint32_t mem_dword = 0xCAFEBABE;
    static_assert(mem_addr + 4 <= RAM_SIZE);

    memcpy(&_ram1->buf[mem_addr], &mem_dword, 4);
    _vm->regs_gp[0] = mem_addr;
    _vm->regs_gp[1] = 0xDEADBEEF;

    _ram1->buf[0] = CPU_OP_LDR_RI0;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1; // mem[R0] -> R1
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], mem_addr);
    EXPECT_EQ(_vm->regs_gp[1], mem_dword);
}

TEST_F(CPUTest, Instr_LDR_RI8) {
    constexpr uint32_t mem_dword = 0xCAFEBABE;
    constexpr uint32_t mem_base = 12;
    constexpr int8_t mem_off = -2;
    static_assert(mem_base + mem_off + 4 <= RAM_SIZE);

    memcpy(&_ram1->buf[mem_base + mem_off], &mem_dword, 4);
    _vm->regs_gp[0] = mem_base;
    _vm->regs_gp[1] = 0xDEADBEEF;

    _ram1->buf[0] = CPU_OP_LDR_RI8; // mem[R0 + imm8] -> R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = (uint8_t)mem_off;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], mem_base);
    EXPECT_EQ(_vm->regs_gp[1], mem_dword);
}

TEST_F(CPUTest, Instr_LDR_RI32) {
    constexpr uint32_t mem_dword = 0xCAFEBABE;
    constexpr uint32_t mem_base = 12;
    constexpr int32_t mem_off = -2;
    static_assert(mem_base + mem_off + 4 <= RAM_SIZE);

    memcpy(&_ram1->buf[mem_base + mem_off], &mem_dword, 4);
    _vm->regs_gp[0] = mem_base;
    _vm->regs_gp[1] = 0xDEADBEEF;

    _ram1->buf[0] = CPU_OP_LDR_RI32; // mem[R0 + imm32] -> R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    memcpy(&_ram1->buf[2], &mem_off, 4);
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], mem_base);
    EXPECT_EQ(_vm->regs_gp[1], mem_dword);
}

TEST_F(CPUTest, Instr_LDR_RIR) {
    constexpr uint32_t mem_dword = 0xCAFEBABE;
    constexpr uint32_t mem_base = 12;
    constexpr int32_t mem_off = -2;
    static_assert(mem_base + mem_off + 4 <= RAM_SIZE);

    memcpy(&_ram1->buf[mem_base + mem_off], &mem_dword, 4);
    _vm->regs_gp[0] = mem_base;
    _vm->regs_gp[1] = 0xDEADBEEF;
    _vm->regs_gp[2] = mem_off;

    _ram1->buf[0] = CPU_OP_LDR_RIR; // mem[R0 + imm32] -> R1
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_CODE_R2;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], mem_base);
    EXPECT_EQ(_vm->regs_gp[1], mem_dword);
}

TEST_F(CPUTest, Instr_ADD_RR) {
    constexpr uint32_t val1 = 0xDEADBEEF;
    constexpr uint32_t val2 = 0xCAFEBABE;
    constexpr uint32_t sum = val1 + val2; // truncated from 0x1_A9AC_79AD
    constexpr uint32_t flags = CPU_FLAG_SIGN | CPU_FLAG_CARRY;

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

TEST_F(CPUTest, Instr_SUB_RR) {
    constexpr uint32_t val1 = 0xCAFEBABE;
    constexpr uint32_t val2 = 0xDEADBEEF;
    constexpr int32_t diff = val1 - val2;
    constexpr uint32_t flags = CPU_FLAG_SIGN | CPU_FLAG_CARRY;

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

TEST_F(CPUTest, Instr_MUL_RR) {
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

TEST_F(CPUTest, Instr_DIV_RR) {
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

TEST_F(CPUTest, Instr_IDIV_RR) {
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

TEST_F(CPUTest, Instr_AND_RR) {
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

TEST_F(CPUTest, Instr_OR_RR) {
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

TEST_F(CPUTest, Instr_XOR_RR) {
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

TEST_F(CPUTest, Instr_NOT_R) {
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

TEST_F(CPUTest, Instr_SHL_RR) {
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

TEST_F(CPUTest, Instr_SHR_RR) {
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

TEST_F(CPUTest, Instr_CMP_RR) {
    constexpr uint32_t val1 = (1 << 31);
    constexpr uint32_t val2 = (1 << 31);
    constexpr uint32_t flags = CPU_FLAG_ZERO | CPU_FLAG_CARRY;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;
    _vm->flags = 0;

    _ram1->buf[0] = CPU_OP_CMP_RR; // R1 - R0, set flags
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    cpu_step(_vm);

    EXPECT_EQ(_vm->regs_gp[0], val1);
    EXPECT_EQ(_vm->regs_gp[1], val2);
    EXPECT_EQ(_vm->flags, flags);
}

TEST_F(CPUTest, Instr_TST_RR) {
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
