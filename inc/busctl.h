#pragma once

#include <stdint.h>

#include "intctl.h"
#include "mem.h"
#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUS_MAX_DEVS 32

#define BUS_DEV_MAP_START 0xFF00'0000
#define BUS_DEV_MAP_END   0xFFFF'F000 // exclusive

/// Device registration request.
typedef struct {
    uint8_t dev_class;
    vm_addr_t region_size;
    void *ctx;
    mem_if_t mem_if;
} busctl_req_t;

/// Registered device context.
typedef struct {
    uint8_t dev_class;
    uint8_t irq_line;
    mmio_region_t mmio;
} busctl_dev_ctx_t;

typedef struct {
    mem_ctx_t *memctl;
    intctl_ctx_t *intctl;

    bool used_slots[BUS_MAX_DEVS];
    busctl_dev_ctx_t devs[BUS_MAX_DEVS];
    size_t num_devs;

    vm_addr_t next_region_at;
    uint8_t next_irq_line;
} busctl_ctx_t;

busctl_ctx_t *busctl_new(mem_ctx_t *mem, intctl_ctx_t *cpu);
void busctl_free(busctl_ctx_t *busctl);

vm_err_t busctl_reg_dev(busctl_ctx_t *busctl, const busctl_req_t *req,
                        const busctl_dev_ctx_t **out_dev_ctx);

#ifdef __cplusplus
}
#endif
