#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "cpu.h"
#include "ram.h"
#include "vm.h"
}

#define RAM_SIZE 128

class CPUStackTest : public testing::Test {
  protected:
    CPUStackTest() {
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
    ~CPUStackTest() {
        vm_free(_vm);
        free(_ram1);
    }

    vm_state_t *_vm;
    ram_t *_ram1;
};

TEST_F(CPUStackTest, Instr_PUSH_V32) {
    constexpr uint32_t val = 0xCAFEBABE;

    _vm->reg_sp = RAM_SIZE;

    _ram1->buf[0] = CPU_OP_PUSH_V32;
    memcpy(&_ram1->buf[1], &val, 4);

    ASSERT_EQ(_vm->reg_sp, RAM_SIZE);
    ASSERT_EQ(_ram1->buf[_vm->reg_sp - 4], 0);
    vm_step(_vm);
    ASSERT_EQ(_vm->reg_sp, RAM_SIZE - 4);

    uint32_t stack_val;
    memcpy(&stack_val, &_ram1->buf[_vm->reg_sp], 4);
    EXPECT_EQ(stack_val, val);
}

TEST_F(CPUStackTest, Instr_PUSH_R) {
    constexpr uint32_t val = 0xCAFEBABE;

    _vm->regs_gp[0] = val;
    _vm->reg_sp = RAM_SIZE;

    _ram1->buf[0] = CPU_OP_PUSH_R;
    _ram1->buf[1] = CPU_CODE_R0;

    ASSERT_EQ(_vm->reg_sp, RAM_SIZE);
    ASSERT_EQ(_ram1->buf[_vm->reg_sp - 4], 0);
    vm_step(_vm);
    ASSERT_EQ(_vm->reg_sp, RAM_SIZE - 4);

    uint32_t stack_val;
    memcpy(&stack_val, &_ram1->buf[_vm->reg_sp], 4);
    EXPECT_EQ(stack_val, val);
}

TEST_F(CPUStackTest, Instr_POP_R) {
    constexpr uint32_t val = 0xCAFEBABE;

    _vm->regs_gp[0] = 0;
    _vm->reg_sp = RAM_SIZE - 4;

    _ram1->buf[0] = CPU_OP_POP_R;
    _ram1->buf[1] = CPU_CODE_R0;
    memcpy(&_ram1->buf[_vm->reg_sp], &val, 4);

    ASSERT_EQ(_vm->reg_sp, RAM_SIZE - 4);
    ASSERT_EQ(_vm->regs_gp[0], 0);
    vm_step(_vm);
    ASSERT_EQ(_vm->reg_sp, RAM_SIZE);
    EXPECT_EQ(_vm->regs_gp[0], val);
}
