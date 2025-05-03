#pragma once

#include <stdint.h>

#include "intctl.h"
#include "memctl.h"
#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum number of devices that can be registered with the bus.
 *
 * Keep in mind that the actual number may be lower due to the limit of
 * memory-mapped regions that the memory controller has (#MEMCTL_MAX_REGIONS).
 */
#define BUS_MAX_DEVS 32

/**
 * @{
 * \name Bus MMIO region description
 *
 * Bus MMIO region is used to convey to the guest code information about which
 * bus slots are used (have devices connected to them) and description of the
 * device at each slot.
 *
 * The MMIO is split into registers:
 * 1. Slot status register (4 bytes).
 * 2. Slot X device descriptor (12 bytes, BUS_MAX_DEVS slots).
 * Hence the formula for the size of the MMIO: (4 + 12 * BUS_MAX_DEVS).
 *
 * For the bit format of each register, see the docs.
 */
#define BUS_MMIO_START 0xF000'0000
#define BUS_MMIO_END   (BUS_MMIO_START + BUS_MMIO_SIZE)

#define BUS_MMIO_DEV_DESC_START 4  //!< Start offset of the device descriptors.
#define BUS_MMIO_DEV_DESC_SIZE  12 //!< Size of a device descriptor.

#define BUS_MMIO_SIZE                                                          \
    (BUS_MMIO_DEV_DESC_START + BUS_MMIO_DEV_DESC_SIZE * BUS_MAX_DEVS)
/// @}

#define BUS_DEV_MAP_START 0xFF00'0000
#define BUS_DEV_MAP_END   0xFFFF'F000 // exclusive

static_assert(BUS_DEV_MAP_START >= BUS_MMIO_END);

/// Device registration request.
typedef struct {
    uint8_t dev_class;
    vm_addr_t region_size;
    void *ctx;
    mem_if_t mem_if;
} busctl_req_t;

/// Registered device context.
typedef struct {
    uint8_t bus_slot;
    uint8_t dev_class;
    uint8_t irq_line;
    mmio_region_t mmio;
} busctl_dev_ctx_t;

typedef struct {
    memctl_ctx_t *memctl;
    intctl_ctx_t *intctl;

    bool used_slots[BUS_MAX_DEVS];
    busctl_dev_ctx_t devs[BUS_MAX_DEVS];
    size_t num_devs;

    vm_addr_t next_region_at;
    uint8_t next_irq_line;

    /// Memory mapped region for accessing the bus registers.
    mmio_region_t bus_mmio;
} busctl_ctx_t;

busctl_ctx_t *busctl_new(memctl_ctx_t *memctl, intctl_ctx_t *intctl);
void busctl_free(busctl_ctx_t *busctl);

vm_err_t busctl_reg_dev(busctl_ctx_t *busctl, const busctl_req_t *req,
                        const busctl_dev_ctx_t **out_dev_ctx);

#ifdef __cplusplus
}
#endif
