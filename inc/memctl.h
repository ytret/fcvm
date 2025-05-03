#pragma once

#include <stddef.h>

#include "vm_err.h"
#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMCTL_MAX_REGIONS 33

typedef vm_err_t (*mem_read_u8_cb)(void *ctx, vm_addr_t addr, uint8_t *out);
typedef vm_err_t (*mem_read_u32_cb)(void *ctx, vm_addr_t addr, uint32_t *out);
typedef vm_err_t (*mem_write_u8_cb)(void *ctx, vm_addr_t addr, uint8_t val);
typedef vm_err_t (*mem_write_u32_cb)(void *ctx, vm_addr_t addr, uint32_t val);

/// Memory interface used by the CPU.
typedef struct {
    mem_read_u8_cb read_u8;
    mem_read_u32_cb read_u32;
    mem_write_u8_cb write_u8;
    mem_write_u32_cb write_u32;
} mem_if_t;

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
vm_err_t memctl_map_region(memctl_ctx_t *memctl, const mmio_region_t *mmio);

vm_err_t memctl_read_u8(void *memctl_ctx, vm_addr_t addr, uint8_t *out);
vm_err_t memctl_read_u32(void *memctl_ctx, vm_addr_t addr, uint32_t *out);
vm_err_t memctl_write_u8(void *memctl_ctx, vm_addr_t addr, uint8_t val);
vm_err_t memctl_write_u32(void *memctl_ctx, vm_addr_t addr, uint32_t val);

#ifdef __cplusplus
}
#endif
