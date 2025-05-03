#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "memctl.h"

static bool prv_mem_find_free_region(memctl_ctx_t *memctl, size_t *out_idx);
static vm_err_t prv_mem_find_mmio_by_addr(memctl_ctx_t *memctl, vm_addr_t addr,
                                          const mmio_region_t **out_reg);

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
    D_ASSERT(mmio->start < mmio->end);
    vm_err_t err = {.type = VM_ERR_NONE};

    // FIXME: there are two iterations over the mapped_regions array, but it
    // could be one if we checked both the start and end in a single iteration.
    if (prv_mem_find_mmio_by_addr(memctl, mmio->start, NULL).type ==
        VM_ERR_NONE) {
        err.type = VM_ERR_MEM_USED;
        return err;
    }
    // The end is exclusive, that's why we check if `end - 1`, the last byte of
    // this region, not `end`, is contained in another region.
    if (prv_mem_find_mmio_by_addr(memctl, mmio->end - 1, NULL).type ==
        VM_ERR_NONE) {
        err.type = VM_ERR_MEM_USED;
        return err;
    }

    size_t idx;
    if (!prv_mem_find_free_region(memctl, &idx)) {
        err.type = VM_ERR_MEM_MAX_REGIONS;
        return err;
    }

    memctl->used_regions[idx] = true;
    memcpy(&memctl->mapped_regions[idx], mmio, sizeof(*mmio));

    return err;
}

vm_err_t memctl_read_u8(void *v_memctl_ctx, vm_addr_t addr, uint8_t *out) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = {.type = VM_ERR_NONE};

    const mmio_region_t *reg;
    err = prv_mem_find_mmio_by_addr(memctl, addr, &reg);
    if (err.type == VM_ERR_NONE) {
        vm_addr_t rel_addr = addr - reg->start;
        err = reg->mem_if.read_u8(reg->ctx, rel_addr, out);
    }

    return err;
}

vm_err_t memctl_read_u32(void *v_memctl_ctx, vm_addr_t addr, uint32_t *out) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = {.type = VM_ERR_NONE};

    const mmio_region_t *reg;
    err = prv_mem_find_mmio_by_addr(memctl, addr, &reg);
    if (err.type == VM_ERR_NONE) {
        if (addr + 4 <= reg->end) {
            vm_addr_t rel_addr = addr - reg->start;
            err = reg->mem_if.read_u32(reg->ctx, rel_addr, out);
        } else {
            err.type = VM_ERR_BAD_MEM;
        }
    }

    return err;
}

vm_err_t memctl_write_u8(void *v_memctl_ctx, vm_addr_t addr, uint8_t val) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = {.type = VM_ERR_NONE};

    const mmio_region_t *reg;
    err = prv_mem_find_mmio_by_addr(memctl, addr, &reg);
    if (err.type == VM_ERR_NONE) {
        vm_addr_t rel_addr = addr - reg->start;
        err = reg->mem_if.write_u8(reg->ctx, rel_addr, val);
    }

    return err;
}

vm_err_t memctl_write_u32(void *v_memctl_ctx, vm_addr_t addr, uint32_t val) {
    D_ASSERT(v_memctl_ctx);
    memctl_ctx_t *memctl = (memctl_ctx_t *)v_memctl_ctx;
    vm_err_t err = {.type = VM_ERR_NONE};

    const mmio_region_t *reg;
    err = prv_mem_find_mmio_by_addr(memctl, addr, &reg);
    if (err.type == VM_ERR_NONE) {
        if (addr + 4 <= reg->end) {
            vm_addr_t rel_addr = addr - reg->start;
            err = reg->mem_if.write_u32(reg->ctx, rel_addr, val);
        } else {
            err.type = VM_ERR_BAD_MEM;
        }
    }

    return err;
}

/**
 * Finds an unused index in the #memctl_ctx_t.mapped_regions array.
 * \param[in] memctl -- Memory controller.
 * \param[out] out_idx -- Output pointer to the unused index.
 * \returns `true` if an unused index was found and written at \a *out_idx,
 *          `false` if no unused index was found and \a *out_idx was not
 *          changed.
 */
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

/**
 * Finds a mapped region that contains address \a addr.
 * \param[in] memctl -- Memory controller.
 * \param[in] addr -- Contained memory address to search by.
 * \param[out] out_reg -- Output pointer to the found memory region (may be
 *                        NULL).
 * \returns #VM_ERR_NONE if a region containing \a addr was found and written
 *          to \a *out_reg (if it's not NULL), #VM_ERR_BAD_MEM if no containing
 *          region was found and \a *out_reg was not written
 */
static vm_err_t prv_mem_find_mmio_by_addr(memctl_ctx_t *memctl, vm_addr_t addr,
                                          const mmio_region_t **out_reg) {
    D_ASSERT(memctl);
    vm_err_t err = {.type = VM_ERR_BAD_MEM};

    for (size_t idx = 0; idx < MEMCTL_MAX_REGIONS; idx++) {
        if (memctl->used_regions[idx]) {
            mmio_region_t reg = memctl->mapped_regions[idx];
            if (reg.start <= addr && addr < reg.end) {
                if (out_reg) { *out_reg = &memctl->mapped_regions[idx]; }
                err.type = VM_ERR_NONE;
                break;
            }
        }
    }

    return err;
}
