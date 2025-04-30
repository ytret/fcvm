#pragma once

#include <stddef.h>

#include "vm_err.h"
#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_MAX_DEVS 32

typedef struct {
    vm_addr_t start;
    vm_addr_t end; // exclusive

    void *ctx;
    vm_err_t (*read_u8)(void *ctx, vm_addr_t addr, uint8_t *out);
    vm_err_t (*read_u32)(void *ctx, vm_addr_t addr, uint32_t *out);
    vm_err_t (*write_u8)(void *ctx, vm_addr_t addr, uint8_t *out);
    vm_err_t (*write_u32)(void *ctx, vm_addr_t addr, uint32_t *out);
} mem_dev_t;

/// Memory interface for the CPU.
typedef struct {
    vm_err_t (*read_u8)(void *ctx, vm_addr_t addr, uint8_t *out);
    vm_err_t (*read_u32)(void *ctx, vm_addr_t addr, uint32_t *out);
    vm_err_t (*write_u8)(void *ctx, vm_addr_t addr, uint8_t val);
    vm_err_t (*write_u32)(void *ctx, vm_addr_t addr, uint32_t val);
} mem_if_t;

typedef struct {
    /// Method table.
    /// Must be the first member so that `mem_t` objects could be casted to
    /// `mem_if_t` and vice versa.
    mem_if_t intf;

    mem_dev_t devs[MEM_MAX_DEVS];
    size_t num_devs;
} mem_ctx_t;

mem_ctx_t *mem_new(void);
void mem_free(mem_ctx_t *mem);
vm_err_t mem_map_dev(mem_ctx_t *mem, const mem_dev_t *dev);

vm_err_t mem_read_u8(void *ctx, vm_addr_t addr, uint8_t *out);
vm_err_t mem_read_u32(void *ctx, vm_addr_t addr, uint32_t *out);
vm_err_t mem_write_u8(void *ctx, vm_addr_t addr, uint8_t val);
vm_err_t mem_write_u32(void *ctx, vm_addr_t addr, uint32_t val);

#ifdef __cplusplus
}
#endif
