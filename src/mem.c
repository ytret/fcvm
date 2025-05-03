#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "mem.h"

static bool prv_mem_find_free_region(mem_ctx_t *mem, size_t *out_idx);

mem_ctx_t *mem_new(void) {
    mem_ctx_t *mem = malloc(sizeof(*mem));
    D_ASSERT(mem);
    memset(mem, 0, sizeof(*mem));

    mem->intf.read_u8 = mem_read_u8;
    mem->intf.read_u32 = mem_read_u32;
    mem->intf.write_u8 = mem_write_u8;
    mem->intf.write_u32 = mem_write_u32;

    return mem;
}

void mem_free(mem_ctx_t *mem) {
    D_ASSERT(mem);
    free(mem);
}

vm_err_t mem_map_region(mem_ctx_t *mem, const mmio_region_t *mmio) {
    D_ASSERT(mem);
    D_ASSERT(mmio);
    vm_err_t err = {.type = VM_ERR_NONE};

    size_t idx;
    if (!prv_mem_find_free_region(mem, &idx)) {
        err.type = VM_ERR_MEM_MAX_REGIONS;
        return err;
    }

    mem->used_regions[idx] = true;
    memcpy(&mem->mapped_regions[idx], mmio, sizeof(*mmio));

    return err;
}

vm_err_t mem_read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t mem_read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t mem_write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}

vm_err_t mem_write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}

static bool prv_mem_find_free_region(mem_ctx_t *mem, size_t *out_idx) {
    D_ASSERT(mem);
    D_ASSERT(out_idx);
    for (size_t idx = 0; idx < MEMCTL_MAX_REGIONS; idx++) {
        if (!mem->used_regions[idx]) {
            *out_idx = idx;
            return true;
        }
    }
    return false;
}
