#include "debugm.h"
#include "mem.h"

vm_res_u8_t mem_read_u8(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);
    vm_res_u8_t res = {.ok = true};

    mmio_t *mmio = vm_find_mmio(vm, addr, 1);
    if (mmio == NULL) {
        D_PRINTF("can't find MMIO for addr 0x%08X, read size 1", addr);
        res.ok = false;
        res.exc.type = VM_EXC_MEM_FAULT;
        return res;
    }

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u8 != NULL);
    res.byte = mmio->pf_read_u8(mmio->ctx, addr_in_mmio);
    return res;
}

vm_res_t mem_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte) {
    D_ASSERT(vm != NULL);
    vm_res_t res = {.ok = true};

    mmio_t *mmio = vm_find_mmio(vm, addr, 1);
    if (mmio == NULL) {
        D_PRINTF("can't find MMIO for addr 0x%08X, write size 1", addr);
        res.ok = false;
        res.exc.type = VM_EXC_MEM_FAULT;
        return res;
    }

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u8 != NULL);
    mmio->pf_write_u8(mmio->ctx, addr_in_mmio, byte);
    return res;
}

vm_res_u32_t mem_read_u32(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);
    vm_res_u32_t res = {.ok = true};

    mmio_t *mmio = vm_find_mmio(vm, addr, 4);
    if (mmio == NULL) {
        D_PRINTF("can't find MMIO for addr 0x%08X, read size 4", addr);
        res.ok = false;
        res.exc.type = VM_EXC_MEM_FAULT;
        return res;
    }

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u32 != NULL);
    res.dword = mmio->pf_read_u32(mmio->ctx, addr_in_mmio);
    return res;
}

vm_res_t mem_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword) {
    D_ASSERT(vm != NULL);
    vm_res_t res = {.ok = true};

    mmio_t *mmio = vm_find_mmio(vm, addr, 4);
    if (mmio == NULL) {
        D_PRINTF("can't find MMIO for addr 0x%08X, write size 4", addr);
        res.ok = false;
        res.exc.type = VM_EXC_MEM_FAULT;
        return res;
    }

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u32 != NULL);
    mmio->pf_write_u32(mmio->ctx, addr_in_mmio, dword);
    return res;
}
