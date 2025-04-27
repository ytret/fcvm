#include <gtest/gtest.h>

extern "C" {
#include "ram.h"
#include "vm.h"
}

#define RAM1_BASE 0
#define RAM1_SIZE 64
#define RAM2_BASE 128
#define RAM2_SIZE 128

class VMStateTest : public testing::Test {
  protected:
    VMStateTest() {
    }
    ~VMStateTest() {
    }

    vm_state_t *create_vm() {
        vm_state_t *vm = vm_new();
        return vm;
    }

    void create_rams(vm_state_t *vm, ram_t **out_ram1, ram_t **out_ram2) {
        ram_t *ram1 = ram_init(RAM1_SIZE);
        ram_t *ram2 = ram_init(RAM2_SIZE);

        mmio_t ram1_mmio = {
            .loaded = true,
            .type = MMIO_RAM,
            .base = RAM1_BASE,
            .size = RAM1_SIZE,
            .ctx = ram1,

            .pf_read_u8 = ram_read_u8,
            .pf_write_u8 = ram_write_u8,
            .pf_read_u32 = ram_read_u32,
            .pf_write_u32 = ram_write_u32,
            .pf_deinit = ram_free,
            .pf_state_size = ram_state_size,
            .pf_state_save = ram_state_save,
        };
        mmio_t ram2_mmio = ram1_mmio;
        ram2_mmio.base = RAM2_BASE;
        ram2_mmio.size = RAM2_SIZE;
        ram2_mmio.ctx = ram2;
        vm_map_device(vm, &ram1_mmio);
        vm_map_device(vm, &ram2_mmio);

        *out_ram1 = ram1;
        *out_ram2 = ram2;
    }
};

TEST_F(VMStateTest, SaveLoadRAM) {
    uint8_t *buf;
    size_t buf_size;
    {
        vm_state_t *vm = create_vm();

        ram_t *ram1;
        ram_t *ram2;
        create_rams(vm, &ram1, &ram2);

        // Allocate a buffer to serialize the state.
        buf_size = vm_state_size(vm);
        buf = (uint8_t *)malloc(buf_size);
        ASSERT_NE(buf, nullptr);

        memset(ram1->buf, 0xAA, ram1->size);
        memset(ram2->buf, 0xEE, ram2->size);

        vm_state_save(vm, buf, buf_size);
        vm_free(vm);
    }

    {
        vm_state_t *vm = vm_load(buf, buf_size);
        size_t buf_offset = 0;

        for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
            mmio_t *mmio = &vm->mmio_regions[idx];
            if (mmio->type == MMIO_RAM) {
                mmio->loaded = true;
                mmio->ctx = ram_load(&buf[buf_offset], buf_size - buf_offset);
            }
        }

        for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
            ASSERT_TRUE(vm->mmio_regions[idx].loaded);
        }
    }

    free(buf);
}
