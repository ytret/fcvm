#include "debugm.h"
#include "ram.h"
#include "vm.h"

#define RAM_SIZE 32

int main(void) {
    vm_state_t *vm = malloc(sizeof(vm_state_t));
    vm_init(vm);
    D_PRINT("vm initialized");

    ram_t *ram1 = malloc(sizeof(ram_t));
    ram_init(ram1, RAM_SIZE);
    D_PRINT("ram1 initialized");

    mmio_t ram1_mmio = {
        .type = MMIO_RAM,
        .base = 0,
        .size = RAM_SIZE,

        .ctx = ram1,
        .pf_read_u32 = ram_read_u32,
        .pf_write_u32 = ram_write_u32,
        .pf_deinit = ram_deinit,
    };

    vm_map_device(vm, &ram1_mmio);
    D_PRINT("ram1 mapped");

    {
        D_PRINT("write to vm");
        uint32_t addr = 4 * RAM_SIZE - 4;
        uint32_t w_dword = 0xDEADBEEF;
        vm_write_u32(vm, addr, w_dword);

        D_PRINT("read from vm");
        uint32_t r_dword = vm_read_u32(vm, addr);
        D_PRINTF("r_dword = 0x%08X", r_dword);
    }

    vm_deinit(vm);
    free(ram1);
    D_PRINT("vm deinitialized");
    return 0;
}
