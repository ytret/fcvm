#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "memctl.h"

static bool prv_mem_find_free_region(memctl_ctx_t *memctl, size_t *out_idx);

memctl_ctx_t *memctl_new(void) {
    memctl_ctx_t *memctl = malloc(sizeof(*memctl));
    D_ASSERT(memctl);
    memset(memctl, 0, sizeof(*memctl));

    memctl->intf.read_u8 = memctl_read_u8;
    memctl->intf.read_u32 = memctl_read_u32;
    memctl->intf.write_u8 = memctl_write_u8;
    memctl->intf.write_u32 = memctl_write_u32;

    return memctl;
}

void memctl_free(memctl_ctx_t *memctl) {
    D_ASSERT(memctl);
    free(memctl);
}

vm_err_t memctl_map_region(memctl_ctx_t *memctl, const mmio_region_t *mmio) {
    D_ASSERT(memctl);
    D_ASSERT(mmio);
    vm_err_t err = {.type = VM_ERR_NONE};

    size_t idx;
    if (!prv_mem_find_free_region(memctl, &idx)) {
        err.type = VM_ERR_MEM_MAX_REGIONS;
        return err;
    }

    memctl->used_regions[idx] = true;
    memcpy(&memctl->mapped_regions[idx], mmio, sizeof(*mmio));

    return err;
}

vm_err_t memctl_read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t memctl_read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t memctl_write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}

vm_err_t memctl_write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}

static bool prv_mem_find_free_region(memctl_ctx_t *memctl, size_t *out_idx) {
    D_ASSERT(memctl);
    D_ASSERT(out_idx);
    for (size_t idx = 0; idx < MEMCTL_MAX_REGIONS; idx++) {
        if (!memctl->used_regions[idx]) {
            *out_idx = idx;
            return true;
        }
    }
    return false;
}
