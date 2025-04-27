#include "debugm.h"
#include "mem.h"

uint8_t mem_read_u8(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = vm_find_mmio(vm, addr, 1);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, read size 1",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u8 != NULL);
    return mmio->pf_read_u8(mmio->ctx, addr_in_mmio);
}

void mem_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = vm_find_mmio(vm, addr, 1);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, write size 1",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u8 != NULL);
    mmio->pf_write_u8(mmio->ctx, addr_in_mmio, byte);
}

uint32_t mem_read_u32(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = vm_find_mmio(vm, addr, 4);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, read size 4",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u32 != NULL);
    return mmio->pf_read_u32(mmio->ctx, addr_in_mmio);
}

void mem_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = vm_find_mmio(vm, addr, 4);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, write size 4",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u32 != NULL);
    mmio->pf_write_u32(mmio->ctx, addr_in_mmio, dword);
}
