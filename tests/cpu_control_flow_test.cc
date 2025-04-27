#include <gtest/gtest.h>

extern "C" {
#include "cpu.h"
#include "ram.h"
#include "vm.h"
}

#define RAM_SIZE 128

class CPUControlFlowTest : public testing::Test {
  protected:
    CPUControlFlowTest() {
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
    ~CPUControlFlowTest() {
        vm_free(_vm);
        free(_ram1);
    }

    vm_state_t *_vm;
    ram_t *_ram1;
};

TEST_F(CPUControlFlowTest, Instr_JMPR_V8) {
    _ram1->buf[0] = CPU_OP_JMPR_V8;
    _ram1->buf[1] = 10;
    _ram1->buf[10] = CPU_OP_JMPR_V8;
    _ram1->buf[11] = -10;

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 10);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 0);
}

TEST_F(CPUControlFlowTest, Instr_JMPA_V32) {
    uint32_t addr1 = 50;
    uint32_t addr2 = 0;

    _ram1->buf[0] = CPU_OP_JMPA_V32;
    memcpy(&_ram1->buf[1], &addr1, 4);
    _ram1->buf[addr1] = CPU_OP_JMPA_V32;
    memcpy(&_ram1->buf[addr1 + 1], &addr2, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr1);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr2);
}

TEST_F(CPUControlFlowTest, Instr_JMPA_R) {
    uint32_t addr1 = 50;
    uint32_t addr2 = 0;

    _vm->regs_gp[0] = addr1;
    _vm->regs_gp[1] = addr2;

    _ram1->buf[0] = CPU_OP_JMPA_R;
    _ram1->buf[1] = CPU_CODE_R0;
    _ram1->buf[addr1] = CPU_OP_JMPA_R;
    _ram1->buf[addr1 + 1] = CPU_CODE_R1;

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr1);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr2);
}

TEST_F(CPUControlFlowTest, Instr_JEQA_V32_Jump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = val1;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JEQA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JEQA_V32_NoJump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = (uint32_t)-10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JEQA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_JNEA_V32_Jump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = (uint32_t)-10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JNEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JNEA_V32_NoJump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = val1;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JNEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_JGTA_V32_Jump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = (uint32_t)-10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JGTA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JGTA_V32_NoJump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = 11;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JGTA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_JGEA_V32_Jump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = 10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JGEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JGEA_V32_NoJump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = 11;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JGEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_JLTA_V32_Jump) {
    constexpr uint32_t val1 = -10;
    constexpr uint32_t val2 = 10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JLTA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JLTA_V32_NoJump) {
    constexpr uint32_t val1 = -10;
    constexpr uint32_t val2 = -10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JLTA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_JLEA_V32_Jump) {
    constexpr uint32_t val1 = 10;
    constexpr uint32_t val2 = 10;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JLEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, addr);
}

TEST_F(CPUControlFlowTest, Instr_JLEA_V32_NoJump) {
    constexpr uint32_t val1 = -10;
    constexpr uint32_t val2 = -11;
    constexpr uint32_t addr = 50;

    _vm->regs_gp[0] = val1;
    _vm->regs_gp[1] = val2;

    _ram1->buf[0] = CPU_OP_CMP_RR;
    _ram1->buf[1] = (CPU_CODE_R0 << 4) | CPU_CODE_R1;
    _ram1->buf[2] = CPU_OP_JLEA_V32;
    memcpy(&_ram1->buf[3], &addr, 4);

    ASSERT_EQ(_vm->reg_pc, 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 2);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, 7);
}

TEST_F(CPUControlFlowTest, Instr_CALLA_V32) {
    constexpr uint32_t stack_top = RAM_SIZE;
    constexpr uint32_t stack_pc_addr = stack_top - 4;
    constexpr uint32_t start_addr = 20;
    constexpr uint32_t fun_addr = 50;

    _vm->reg_pc = start_addr;
    _vm->reg_sp = stack_top;

    _ram1->buf[start_addr] = CPU_OP_CALLA_V32;
    memcpy(&_ram1->buf[start_addr + 1], &fun_addr, 4);
    constexpr uint32_t instr_size = 5;

    ASSERT_EQ(_vm->reg_pc, start_addr);
    ASSERT_EQ(vm_read_u32(_vm, stack_pc_addr), 0);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, fun_addr);
    ASSERT_EQ(vm_read_u32(_vm, stack_pc_addr), start_addr + instr_size);
}

TEST_F(CPUControlFlowTest, Instr_CALLA_V32_Then_RET) {
    constexpr uint32_t stack_top = RAM_SIZE;
    constexpr uint32_t start_addr = 20;
    constexpr uint32_t fun_addr = 50;

    _vm->reg_pc = start_addr;
    _vm->reg_sp = stack_top;

    // Start function.
    _ram1->buf[start_addr] = CPU_OP_CALLA_V32;
    memcpy(&_ram1->buf[start_addr + 1], &fun_addr, 4);
    constexpr uint32_t call_instr_size = 5;

    // Called function.
    _ram1->buf[fun_addr] = CPU_OP_RET;

    ASSERT_EQ(_vm->reg_pc, start_addr);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, fun_addr);
    cpu_step(_vm);
    ASSERT_EQ(_vm->reg_pc, start_addr + call_instr_size);
}
