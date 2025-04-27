#include <string.h>

#include "cpu.h"
#include "debugm.h"
#include "vm.h"

static mmio_t *prv_vm_find_mmio(vm_state_t *vm, uint32_t addr,
                                uint32_t access_size);

vm_state_t *vm_new(void) {
    vm_state_t *vm = malloc(sizeof(vm_state_t));
    D_ASSERT(vm != NULL);
    memset(vm, 0, sizeof(vm_state_t));
    return vm;
}

vm_state_t *vm_load(void *v_buf, size_t buf_size) {
    vm_state_t *vm = (vm_state_t *)malloc(sizeof(vm_state_t));
    D_ASSERT(vm != NULL);

    uint8_t *buf = (uint8_t *)v_buf;
    size_t buf_offset = 0;

    D_ASSERT(buf_size >= sizeof(vm_state_t));
    memcpy(vm, &buf[buf_offset], sizeof(vm_state_t));
    buf_offset += sizeof(vm_state_t);

    for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
        D_ASSERT(buf_offset < buf_size);
        mmio_t *mmio = &vm->mmio_regions[idx];
        memset(mmio, 0, sizeof(mmio_t));
        mmio->loaded = false;
    }

    return vm;
}

void vm_free(vm_state_t *vm) {
    D_ASSERT(vm != NULL);

    for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
        mmio_t *mmio = &vm->mmio_regions[idx];
        D_ASSERT(mmio->pf_deinit);
        mmio->pf_deinit(mmio->ctx);
    }

    free(vm);
}

size_t vm_state_size(const vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    size_t size = sizeof(vm_state_t);
    for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
        const mmio_t *mmio = &vm->mmio_regions[idx];
        D_ASSERT(mmio->pf_state_size);
        size_t mmio_size = mmio->pf_state_size(mmio->ctx);
        size += mmio_size;
    }
    return size;
}

void vm_state_save(const vm_state_t *vm, void *v_buf, size_t buf_size) {
    D_ASSERT(vm != NULL);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t buf_offset = 0;

    D_ASSERT(buf_size >= sizeof(vm_state_t));
    memcpy(buf, vm, sizeof(vm_state_t));
    buf_offset += sizeof(vm_state_t);

    for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
        const mmio_t *mmio = &vm->mmio_regions[idx];
        D_ASSERT(mmio->pf_state_save);
        D_ASSERT(buf_offset < buf_size);
        buf_offset += mmio->pf_state_save(mmio->ctx, &buf[buf_offset],
                                          buf_size - buf_offset);
    }
}

void vm_step(vm_state_t *vm) {
    cpu_step(vm);
}

void vm_map_device(vm_state_t *vm, const mmio_t *mmio) {
    D_ASSERT(vm != NULL);
    D_ASSERT(vm->mmio_count < VM_MAX_MMIO_REGIONS);

    D_ASSERT(mmio != NULL);
    D_ASSERT(mmio->base % 4 == 0);
    D_ASSERT(mmio->size % 4 == 0);

    memcpy(&vm->mmio_regions[vm->mmio_count], mmio, sizeof(mmio_t));
    vm->mmio_count++;
}

uint8_t vm_read_u8(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = prv_vm_find_mmio(vm, addr, 1);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, read size 1",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u8 != NULL);
    return mmio->pf_read_u8(mmio->ctx, addr_in_mmio);
}

void vm_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = prv_vm_find_mmio(vm, addr, 1);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, write size 1",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u8 != NULL);
    mmio->pf_write_u8(mmio->ctx, addr_in_mmio, byte);
}

uint32_t vm_read_u32(vm_state_t *vm, uint32_t addr) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = prv_vm_find_mmio(vm, addr, 4);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, read size 4",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_read_u32 != NULL);
    return mmio->pf_read_u32(mmio->ctx, addr_in_mmio);
}

void vm_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword) {
    D_ASSERT(vm != NULL);

    mmio_t *mmio = prv_vm_find_mmio(vm, addr, 4);
    // TODO: raise exception
    D_ASSERTMF(mmio != NULL, "can't find MMIO for addr 0x%08X, write size 4",
               addr);

    uint32_t addr_in_mmio = addr - mmio->base;
    D_ASSERT(mmio->pf_write_u32 != NULL);
    mmio->pf_write_u32(mmio->ctx, addr_in_mmio, dword);
}

static mmio_t *prv_vm_find_mmio(vm_state_t *vm, uint32_t addr,
                                uint32_t access_size) {
    D_ASSERT(vm != NULL);

    uint32_t access_start = addr;
    uint32_t access_end_excl = addr + access_size;

    for (uint32_t idx = 0; idx < vm->mmio_count; idx++) {
        mmio_t *mmio = &vm->mmio_regions[idx];
        uint32_t mmio_end_excl = mmio->base + mmio->size;
        bool has_start =
            mmio->base <= access_start && access_start < mmio_end_excl;
        bool has_end =
            mmio->base <= access_end_excl && access_end_excl <= mmio_end_excl;
        if (has_start && has_end) {
            D_ASSERTM(mmio->loaded, "MMIO is found, but it's not loaded");
            return mmio;
        }
    }

    return NULL;
}
