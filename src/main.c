#include "debugm.h"
#include "ram.h"
#include "vm.h"

#define RAM_SIZE 32

int main(void) {
    vm_state_t *vm = vm_new();
    D_PRINT("vm initialized");

    ram_t *ram1 = ram_init(RAM_SIZE);
    D_PRINT("ram1 initialized");

    mmio_t ram1_mmio = {
        .type = MMIO_RAM,
        .base = 0,
        .size = RAM_SIZE,

        .ctx = ram1,
        .pf_read_u32 = ram_read_u32,
        .pf_write_u32 = ram_write_u32,
        .pf_deinit = ram_free,
    };

    vm_map_device(vm, &ram1_mmio);
    D_PRINT("ram1 mapped");

    vm_free(vm);
    free(ram1);
    D_PRINT("vm deinitialized");
    return 0;
}
