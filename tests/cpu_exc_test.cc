#include <gtest/gtest.h>

extern "C" {
#include "cpu.h"
#include "ram.h"
#include "vm.h"
}

#define RAM_BASE 0xFFFF0000
#define RAM_SIZE 0x1000

class CPUExcTest : public testing::Test {
  protected:
    CPUExcTest() {
        _vm = vm_new();
    }
    ~CPUExcTest() {
        vm_free(_vm);
    }

    void init_ram() {
        _ram = ram_init(RAM_SIZE);
        ram_clear(_ram);
        mmio_t ram_mmio = {
            .loaded = true,
            .type = MMIO_RAM,
            .base = RAM_BASE,
            .size = RAM_SIZE,
            .ctx = _ram,

            .pf_read_u8 = ram_read_u8,
            .pf_write_u8 = ram_write_u8,
            .pf_read_u32 = ram_read_u32,
            .pf_write_u32 = ram_write_u32,
            .pf_deinit = ram_free,
            .pf_state_size = ram_state_size,
            .pf_state_save = ram_state_save,
        };
        vm_map_device(_vm, &ram_mmio);
    }

    vm_state_t *_vm;
    ram_t *_ram = nullptr;
};

TEST_F(CPUExcTest, NoExc) {
    init_ram();
    _ram->buf[0] = CPU_OP_PUSH_R;
    _ram->buf[1] = CPU_CODE_R0;
    _vm->reg_pc = RAM_BASE;
    vm_step(_vm);
    EXPECT_EQ(_vm->interrupt_depth, 0);
}

TEST_F(CPUExcTest, ExecBadMem) {
    _vm->reg_pc = 0;
    vm_step(_vm);
    EXPECT_EQ(_vm->interrupt_depth, 1);
}
