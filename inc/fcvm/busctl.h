#pragma once

#include <stdint.h>

#include <fcvm/intctl.h>
#include <fcvm/memctl.h>
#include <fcvm/vm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Version of the `busctl_ctx_t` structure and its member structures.
/// Increment this every time anything in the `busctl_ctx_t` structure or its
/// member structures is changed: field order, size, type, etc.
#define SN_BUSCTL_CTX_VER ((uint32_t)1)

/**
 * Maximum number of devices that can be registered with the bus.
 *
 * Keep in mind that the actual number may be lower due to the limit of
 * memory-mapped regions that the memory controller has (#MEMCTL_MAX_REGIONS).
 */
#define BUS_MAX_DEVS 32

/**
 * @{
 * @anchor DbusMMIO
 * @name Bus MMIO region description
 *
 * Bus MMIO region is used to convey to the guest code information about which
 * bus slots are used (have devices connected to them) and description of a
 * device at each slot.
 *
 * The MMIO is split into registers:
 * 1. Slot status register (4 bytes).
 * 2. Slot X device descriptor (12 bytes, #BUS_MAX_DEVS slots).
 *
 * Hence the formula for the size of the MMIO: (4 + 12 * #BUS_MAX_DEVS).
 *
 * For the bit format of each register, see the docs spreadsheet.
 */
/// Start address of the bus MMIO region.
#define BUS_MMIO_START 0xF0000000
/// End address (exclusive) of the bus MMIO region.
#define BUS_MMIO_END   (BUS_MMIO_START + BUS_MMIO_SIZE)

/// Byte offset of the first device descriptor.
#define BUS_MMIO_DEV_DESC_START 4
/// Size in bytes of a device descriptor register.
#define BUS_MMIO_DEV_DESC_SIZE  12

/// Size in bytes of the bus MMIO region.
#define BUS_MMIO_SIZE                                                          \
    (BUS_MMIO_DEV_DESC_START + BUS_MMIO_DEV_DESC_SIZE * BUS_MAX_DEVS)
/// @}

/**
 * Start address for assigning MMIO regions for devices.
 * Keep in mind that the device MMIO mapping region should fully include the CPU
 * IVT, otherwise some or all entries of the IVT won't be accessible to the CPU.
 */
#define BUS_DEV_MAP_START 0x00000000
/// End address (exclusive) of the device MMIO regions.
#define BUS_DEV_MAP_END   0xF0000000

static_assert(BUS_MMIO_START >= BUS_DEV_MAP_END);

/// Connected device context.
struct busctl_dev_ctx {
    /// Device index in the VM.
    /// Propagated to the guest program using the bus MMIO.
    /// See #busctl_ctx_t.used_slots and #busctl_ctx_t.devs.
    uint8_t bus_slot;
    /// Device class.
    /// Propagated to the guest program using the bus MMIO. Used for restoral of
    /// the device context during VM restoral by passing it to the
    /// #cb_restore_dev_t argument of #vm_restore().
    uint8_t dev_class;
    uint8_t irq_line;
    mmio_region_t mmio;

    void *snapshot_ctx;
    cb_snapshot_size_dev_t f_snapshot_size;
    cb_snapshot_dev_t f_snapshot;
};

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

/**
 * Creates a new bus controller.
 *
 * Allocates a single MMIO region in the @a memctl controller for @ref DbusMMIO
 * "the bus MMIO region". See #busctl_new_in_reg().
 *
 * @param memctl Memory controller used for mapping @ref DbusMMIO
 *               "the bus MMIO region" and MMIO regions assigned to connected
 *               devices.
 * @param intctl Interrupt controller, IRQ lines of which are assigned to
 *               connected devices.
 *
 * @returns A newly allocated #busctl_ctx_t structure, never `NULL`.
 */
busctl_ctx_t *busctl_new(memctl_ctx_t *memctl, intctl_ctx_t *intctl);

/**
 * Creates a new bus controller using the @a in_reg region for the bus MMIO.
 *
 * Contrary to #busctl_new(), this initializer does not allocate a new MMIO
 * region. Instead, it uses the @a in_reg region for @ref DbusMMIO
 * "the bus MMIO".
 *
 * @param memctl Memory controller used for mapping connected devices.
 * @param intctl Interrupt controller, IRQ lines of which are assigned to
 *               connected deviecs.
 * @param in_reg MMIO region in @a memctl that will be used for the bus MMIO
 *               operations.
 *
 * @returns A newly allocated #busctl_ctx_t structure, never `NULL`.
 */
busctl_ctx_t *busctl_new_in_reg(memctl_ctx_t *memctl, intctl_ctx_t *intctl,
                                mmio_region_t *in_reg);

/**
 * Frees memory used by the @a busctl structure.
 * Does not deinitialize #busctl_ctx_t.memctl or #busctl_ctx_t.intctl, nor does
 * not unmap connected devices or deallocate their IRQ lines.
 * @param busctl Bus controller (must not be `NULL`).
 */
void busctl_free(busctl_ctx_t *busctl);

size_t busctl_snapshot_size(const busctl_ctx_t *busctl);
size_t busctl_snapshot(const busctl_ctx_t *busctl, void *v_buf,
                       size_t max_size);
busctl_ctx_t *busctl_restore(memctl_ctx_t *memctl, intctl_ctx_t *intctl,
                             cb_restore_dev_t f_restore_dev, const void *v_buf,
                             size_t max_size, size_t *out_used_size);

vm_err_t busctl_connect_dev(busctl_ctx_t *busctl, const dev_desc_t *desc,
                            void *ctx, const busctl_dev_ctx_t **out_dev_ctx);

#ifdef __cplusplus
}
#endif
