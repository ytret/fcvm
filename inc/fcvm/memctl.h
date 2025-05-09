#pragma once

#include <fcvm/vm_err.h>
#include <fcvm/vm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Version of the `memctl_ctx_t` structure and its member structures.
/// Increment this every time anything in the `memctl_ctx_t` structure or its
/// member structures is changed: field order, size, type, etc.
#define SN_MEMCTL_CTX_VER ((uint32_t)1)

#define MEMCTL_MAX_REGIONS 33

typedef struct {
    vm_addr_t start;
    vm_addr_t end; // exclusive

    void *ctx;
    mem_if_t mem_if;
} mmio_region_t;

typedef struct {
    /// Function table (interface).
    /// Must be the first field so that `mem_ctx_t` objects could be casted to
    /// `mem_if_t` and vice versa.
    mem_if_t intf;

    bool used_regions[MEMCTL_MAX_REGIONS];
    mmio_region_t mapped_regions[MEMCTL_MAX_REGIONS];
    size_t num_mapped_regions;
} memctl_ctx_t;

memctl_ctx_t *memctl_new(void);
void memctl_free(memctl_ctx_t *memctl);
size_t memctl_snapshot_size(void);
size_t memctl_snapshot(const memctl_ctx_t *memctl, void *buf, size_t max_size);
memctl_ctx_t *memctl_restore(const void *buf, size_t max_size,
                             size_t *out_used_size);

vm_err_t memctl_map_region(memctl_ctx_t *memctl, const mmio_region_t *mmio);

/**
 * Finds a mapped region that contains address \a addr.
 * \param[in] memctl -- Memory controller.
 * \param[in] addr -- Contained memory address to search by.
 * \param[out] out_reg -- Output pointer to the found memory region (may be
 *                        NULL).
 * \returns #VM_ERR_NONE if a region containing \a addr was found and
 * written to \a *out_reg (if it's not NULL), #VM_ERR_BAD_MEM if no
 * containing region was found and \a *out_reg was not written
 */
vm_err_t memctl_find_reg_by_addr(memctl_ctx_t *memctl, vm_addr_t addr,
                                 mmio_region_t **out_reg);

vm_err_t memctl_read_u8(void *memctl_ctx, vm_addr_t addr, uint8_t *out);
vm_err_t memctl_read_u32(void *memctl_ctx, vm_addr_t addr, uint32_t *out);
vm_err_t memctl_write_u8(void *memctl_ctx, vm_addr_t addr, uint8_t val);
vm_err_t memctl_write_u32(void *memctl_ctx, vm_addr_t addr, uint32_t val);

#ifdef __cplusplus
}
#endif
