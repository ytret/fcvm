/**
 * @file memctl.c
 * Memory controller implementation.
 */

#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include <fcvm/memctl.h>

static bool prv_memctl_find_free_reg(memctl_ctx_t *memctl, size_t *out_idx);

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

size_t memctl_snapshot_size(void) {
    static_assert(SN_MEMCTL_CTX_VER == 1);
    return sizeof(memctl_ctx_t);
}

size_t memctl_snapshot(const memctl_ctx_t *memctl, void *v_buf,
                       size_t max_size) {
    static_assert(SN_MEMCTL_CTX_VER == 1);
    D_ASSERT(memctl);
    D_ASSERT(v_buf);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t size = 0;

    // Replace every pointer by NULL.
    memctl_ctx_t memctl_copy;
    memcpy(&memctl_copy, memctl, sizeof(memctl_copy));
    memctl_copy.intf.read_u8 = NULL;
    memctl_copy.intf.read_u32 = NULL;
    memctl_copy.intf.write_u8 = NULL;
    memctl_copy.intf.write_u32 = NULL;
    for (size_t idx = 0; idx < MEMCTL_MAX_REGIONS; idx++) {
        mmio_region_t *reg = &memctl_copy.mapped_regions[idx];
        reg->ctx = NULL;
        reg->mem_if.read_u8 = NULL;
        reg->mem_if.read_u32 = NULL;
        reg->mem_if.write_u8 = NULL;
        reg->mem_if.write_u32 = NULL;
    }

    // Write the memctl context.
    D_ASSERT(size + sizeof(memctl_copy) <= max_size);
    memcpy(&buf[size], &memctl_copy, sizeof(memctl_copy));
    size += sizeof(memctl_copy);

    return size;
}

memctl_ctx_t *memctl_restore(const void *v_buf, size_t max_size,
                             size_t *out_used_size) {
    static_assert(SN_MEMCTL_CTX_VER == 1);
    D_ASSERT(v_buf);
    D_ASSERT(out_used_size);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t offset = 0;

    // Restore the memctl context.
    memctl_ctx_t rest_memctl;
    D_ASSERT(offset + sizeof(rest_memctl) <= max_size);
    memcpy(&rest_memctl, &buf[offset], sizeof(rest_memctl));
    offset += sizeof(rest_memctl);

    // Create a new memctl and set the fields manually.
    // memctl_new() sets the memctl's interface pointers.
    memctl_ctx_t *memctl = memctl_new();
    memcpy(memctl->used_regions, rest_memctl.used_regions,
           sizeof(rest_memctl.used_regions));
    memcpy(memctl->mapped_regions, rest_memctl.mapped_regions,
           sizeof(rest_memctl.mapped_regions));
    memctl->num_mapped_regions = rest_memctl.num_mapped_regions;

    // The caller must now restore the context and interface of each region.

    *out_used_size = offset;
    return memctl;
}

vm_err_t memctl_map_region(memctl_ctx_t *memctl, const mmio_region_t *mmio) {
    D_ASSERT(memctl);
    D_ASSERT(mmio);
    D_ASSERT(mmio->start < mmio->end);
    vm_err_t err = VM_ERR_NONE;

    // FIXME: there are two iterations over the mapped_regions array, but it
    // could be one if we checked both the start and end in a single iteration.
    if (memctl_find_reg_by_addr(memctl, mmio->start, NULL) == VM_ERR_NONE) {
        err = VM_ERR_MEM_USED;
        return err;
    }
    // The end is exclusive, that's why we check if `end - 1`, the last byte of
    // this region, not `end`, is contained in another region.
    if (memctl_find_reg_by_addr(memctl, mmio->end - 1, NULL) == VM_ERR_NONE) {
        err = VM_ERR_MEM_USED;
        return err;
    }

    size_t idx;
    if (!prv_memctl_find_free_reg(memctl, &idx)) {
        err = VM_ERR_MEM_MAX_REGIONS;
        return err;
    }

    memctl->used_regions[idx] = true;
    memcpy(&memctl->mapped_regions[idx], mmio, sizeof(*mmio));

    return err;
}

vm_err_t memctl_find_reg_by_addr(memctl_ctx_t *memctl, vm_addr_t addr,
                                 mmio_region_t **out_reg) {
    D_ASSERT(memctl);
    vm_err_t err = VM_ERR_BAD_MEM;

    for (size_t idx = 0; idx < MEMCTL_MAX_REGIONS; idx++) {
        if (memctl->used_regions[idx]) {
            mmio_region_t reg = memctl->mapped_regions[idx];
            if (reg.start <= addr && addr < reg.end) {
                if (out_reg) { *out_reg = &memctl->mapped_regions[idx]; }
                err = VM_ERR_NONE;
                break;
            }
        }
    }

    return err;
}

vm_err_t memctl_read_u8(void *v_memctl_ctx, vm_addr_t addr, uint8_t *out) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = VM_ERR_NONE;

    mmio_region_t *reg;
    err = memctl_find_reg_by_addr(memctl, addr, &reg);
    if (err == VM_ERR_NONE) {
        if (reg->mem_if.read_u8) {
            vm_addr_t rel_addr = addr - reg->start;
            err = reg->mem_if.read_u8(reg->ctx, rel_addr, out);
        } else {
            err = VM_ERR_MEM_BAD_OP;
        }
    }

    return err;
}

vm_err_t memctl_read_u32(void *v_memctl_ctx, vm_addr_t addr, uint32_t *out) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = VM_ERR_NONE;

    mmio_region_t *reg;
    err = memctl_find_reg_by_addr(memctl, addr, &reg);
    if (err == VM_ERR_NONE) {
        if (reg->mem_if.read_u32) {
            if (addr + 4 <= reg->end) {
                vm_addr_t rel_addr = addr - reg->start;
                err = reg->mem_if.read_u32(reg->ctx, rel_addr, out);
            } else {
                err = VM_ERR_BAD_MEM;
            }
        } else {
            err = VM_ERR_MEM_BAD_OP;
        }
    }

    return err;
}

vm_err_t memctl_write_u8(void *v_memctl_ctx, vm_addr_t addr, uint8_t val) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = VM_ERR_NONE;

    mmio_region_t *reg;
    err = memctl_find_reg_by_addr(memctl, addr, &reg);
    if (err == VM_ERR_NONE) {
        if (reg->mem_if.write_u8) {
            vm_addr_t rel_addr = addr - reg->start;
            err = reg->mem_if.write_u8(reg->ctx, rel_addr, val);
        } else {
            err = VM_ERR_MEM_BAD_OP;
        }
    }

    return err;
}

vm_err_t memctl_write_u32(void *v_memctl_ctx, vm_addr_t addr, uint32_t val) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = VM_ERR_NONE;

    mmio_region_t *reg;
    err = memctl_find_reg_by_addr(memctl, addr, &reg);
    if (err == VM_ERR_NONE) {
        if (reg->mem_if.write_u32) {
            if (addr + 4 <= reg->end) {
                vm_addr_t rel_addr = addr - reg->start;
                err = reg->mem_if.write_u32(reg->ctx, rel_addr, val);
            } else {
                err = VM_ERR_BAD_MEM;
            }
        } else {
            err = VM_ERR_MEM_BAD_OP;
        }
    }

    return err;
}

/**
 * Finds an unused index in the #memctl_ctx_t.mapped_regions array.
 * @param[in]  memctl  Memory controller.
 * @param[out] out_idx Output pointer to the unused index.
 * @returns `true` if an unused index was found and written at @a *out_idx,
 *          `false` if no unused index was found and @a *out_idx was not
 *          changed.
 */
static bool prv_memctl_find_free_reg(memctl_ctx_t *memctl, size_t *out_idx) {
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
