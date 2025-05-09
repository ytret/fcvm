/**
 * @file busctl.c
 * Bus controller implementation.
 */

#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include <fcvm/busctl.h>

static bool prv_busctl_find_free_slot(busctl_ctx_t *busctl, size_t *out_idx);

static vm_err_t prv_busctl_mmio_read_u32(void *ctx, vm_addr_t addr,
                                         uint32_t *out_val);

busctl_ctx_t *busctl_new(memctl_ctx_t *memctl, intctl_ctx_t *intctl) {
    return busctl_new_in_reg(memctl, intctl, NULL);
}

busctl_ctx_t *busctl_new_in_reg(memctl_ctx_t *memctl, intctl_ctx_t *intctl,
                                mmio_region_t *in_reg) {
    D_ASSERT(memctl);
    D_ASSERT(intctl);

    busctl_ctx_t *busctl = malloc(sizeof(*busctl));
    D_ASSERT(busctl);
    memset(busctl, 0, sizeof(*busctl));

    busctl->memctl = memctl;
    busctl->intctl = intctl;
    busctl->next_region_at = BUS_DEV_MAP_START;
    busctl->next_irq_line = 0;

    // Map the bus MMIO.
    busctl->bus_mmio.start = BUS_MMIO_START;
    busctl->bus_mmio.end = BUS_MMIO_END;
    busctl->bus_mmio.ctx = busctl;
    busctl->bus_mmio.mem_if.read_u8 = NULL;
    busctl->bus_mmio.mem_if.read_u32 = prv_busctl_mmio_read_u32;
    busctl->bus_mmio.mem_if.write_u8 = NULL;
    busctl->bus_mmio.mem_if.write_u32 = NULL;
    if (in_reg) {
        memcpy(in_reg, &busctl->bus_mmio, sizeof(mmio_region_t));
    } else {
        vm_err_t err = memctl_map_region(memctl, &busctl->bus_mmio);
        D_ASSERTMF(err == VM_ERR_NONE,
                   "failed to map the bus MMIO, error type: %u", err);
    }

    return busctl;
}

void busctl_free(busctl_ctx_t *busctl) {
    D_ASSERT(busctl);
    free(busctl);
}

size_t busctl_snapshot_size(const busctl_ctx_t *busctl) {
    static_assert(SN_BUSCTL_CTX_VER == 1);
    size_t size = sizeof(busctl_ctx_t);
    for (size_t idx = 0; idx < BUS_MAX_DEVS; idx++) {
        const busctl_dev_ctx_t *dev = &busctl->devs[idx];
        if (busctl->used_slots[idx] && dev->f_snapshot_size) {
            size += dev->f_snapshot_size(dev->snapshot_ctx);
        }
    }
    return size;
}

size_t busctl_snapshot(const busctl_ctx_t *busctl, void *v_buf,
                       size_t max_size) {
    static_assert(SN_BUSCTL_CTX_VER == 1);
    D_ASSERT(busctl);
    D_ASSERT(v_buf);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t size = 0;

    // Replace every pointer by NULL.
    busctl_ctx_t busctl_copy;
    memcpy(&busctl_copy, busctl, sizeof(busctl_copy));
    busctl_copy.memctl = NULL;
    busctl_copy.intctl = NULL;
    for (size_t idx = 0; idx < BUS_MAX_DEVS; idx++) {
        busctl_copy.devs[idx].mmio.ctx = NULL;
        busctl_copy.devs[idx].mmio.mem_if.read_u8 = NULL;
        busctl_copy.devs[idx].mmio.mem_if.read_u32 = NULL;
        busctl_copy.devs[idx].mmio.mem_if.write_u8 = NULL;
        busctl_copy.devs[idx].mmio.mem_if.write_u32 = NULL;
        busctl_copy.devs[idx].snapshot_ctx = NULL;
        busctl_copy.devs[idx].f_snapshot_size = NULL;
        busctl_copy.devs[idx].f_snapshot = NULL;
    }
    busctl_copy.bus_mmio.mem_if.read_u8 = NULL;
    busctl_copy.bus_mmio.mem_if.read_u32 = NULL;
    busctl_copy.bus_mmio.mem_if.write_u8 = NULL;
    busctl_copy.bus_mmio.mem_if.write_u32 = NULL;

    // Write the context.
    D_ASSERT(size + sizeof(busctl_copy) <= max_size);
    memcpy(&buf[size], &busctl_copy, sizeof(busctl_copy));
    size += sizeof(busctl_copy);

    // Snapshot every connected device.
    for (size_t idx = 0; idx < BUS_MAX_DEVS; idx++) {
        const busctl_dev_ctx_t *dev = &busctl->devs[idx];
        if (busctl->used_slots[idx] && dev->f_snapshot) {
            size +=
                dev->f_snapshot(dev->snapshot_ctx, &buf[size], max_size - size);
        }
    }

    return size;
}

busctl_ctx_t *busctl_restore(memctl_ctx_t *memctl, intctl_ctx_t *intctl,
                             cb_restore_dev_t f_restore_dev, const void *v_buf,
                             size_t max_size, size_t *out_used_size) {
    static_assert(SN_BUSCTL_CTX_VER == 1);
    D_ASSERT(memctl);
    D_ASSERT(intctl);
    D_ASSERT(f_restore_dev);
    D_ASSERT(v_buf);
    D_ASSERT(out_used_size);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t offset = 0;

    // Find the busctl MMIO region in the memory controller.
    mmio_region_t *bus_mmio = NULL;
    vm_err_t err = memctl_find_reg_by_addr(memctl, BUS_MMIO_START, &bus_mmio);
    D_ASSERT(err == VM_ERR_NONE);
    D_ASSERT(bus_mmio);

    // Restore the busctl context.
    busctl_ctx_t *busctl = busctl_new_in_reg(memctl, intctl, bus_mmio);
    D_ASSERT(offset + sizeof(*busctl) <= max_size);
    memcpy(busctl, &buf[offset], sizeof(*busctl));
    offset += sizeof(*busctl);

    // Restore the devices.
    for (size_t idx = 0; idx < BUS_MAX_DEVS; idx++) {
        if (busctl->used_slots[idx]) {
            // Restore the device entry in busctl.
            uint8_t dev_class = busctl->devs[idx].dev_class;
            offset += f_restore_dev(dev_class, &busctl->devs[idx], &buf[offset],
                                    max_size - offset);

            // Restore the ctx and mem interface in memctl.
            mmio_region_t *memctl_reg = NULL;
            vm_err_t err = memctl_find_reg_by_addr(
                memctl, busctl->devs[idx].mmio.start, &memctl_reg);
            D_ASSERT(err == VM_ERR_NONE);
            D_ASSERT(memctl_reg);
            memcpy(memctl_reg, &busctl->devs[idx].mmio, sizeof(*memctl_reg));
        }
    }

    *out_used_size = offset;
    return busctl;
}

vm_err_t busctl_connect_dev(busctl_ctx_t *busctl, const dev_desc_t *desc,
                            void *ctx, const busctl_dev_ctx_t **out_dev_ctx) {
    D_ASSERT(busctl);
    D_ASSERT(desc);
    vm_err_t err = VM_ERR_NONE;

    // Find a free device slot.
    size_t slot;
    if (!prv_busctl_find_free_slot(busctl, &slot)) {
        err = VM_ERR_BUS_NO_FREE_SLOT;
        return err;
    }

    // Allocate resources for the device, but don't commit them yet. Commitment
    // is done at the end of this function if the connection is successful.
    uint8_t irq_line = busctl->next_irq_line;
    static_assert(BUS_MAX_DEVS < UINT8_MAX,
                  "cannot support this much devices because of the IRQ limit");
    vm_addr_t map_start = busctl->next_region_at;
    vm_addr_t map_end = map_start + desc->region_size;
    if (map_end >= BUS_DEV_MAP_END) {
        err = VM_ERR_BUS_NO_FREE_MEM;
        return err;
    }

    // Map the region.
    mmio_region_t mmio = {
        .start = map_start,
        .end = map_end,
        .ctx = ctx,
        .mem_if = desc->mem_if,
    };
    err = memctl_map_region(busctl->memctl, &mmio);
    if (err != VM_ERR_NONE) { return err; }

    // Lock the slot and fill the device context.
    busctl->used_slots[slot] = true;
    busctl_dev_ctx_t *dev_ctx = &busctl->devs[slot];
    memset(dev_ctx, 0, sizeof(*dev_ctx));
    dev_ctx->bus_slot = slot;
    dev_ctx->dev_class = desc->dev_class;
    dev_ctx->irq_line = irq_line;
    dev_ctx->mmio = mmio;
    dev_ctx->snapshot_ctx = ctx;
    dev_ctx->f_snapshot_size = desc->f_snapshot_size;
    dev_ctx->f_snapshot = desc->f_snapshot;
    if (out_dev_ctx) { *out_dev_ctx = dev_ctx; }

    D_ASSERT(err == VM_ERR_NONE);
    busctl->next_irq_line++;
    busctl->next_region_at = map_end;
    return err;
}

static bool prv_busctl_find_free_slot(busctl_ctx_t *busctl, size_t *out_idx) {
    D_ASSERT(busctl);
    D_ASSERT(out_idx);
    for (size_t slot = 0; slot < BUS_MAX_DEVS; slot++) {
        if (!busctl->used_slots[slot]) {
            *out_idx = slot;
            return true;
        }
    }
    return false;
}

static vm_err_t prv_busctl_mmio_read_u32(void *v_ctx, vm_addr_t offset,
                                         uint32_t *out_val) {
    D_ASSERT(v_ctx);
    D_ASSERT(out_val);
    const busctl_ctx_t *busctl = (busctl_ctx_t *)v_ctx;

    vm_err_t err = VM_ERR_NONE;

    if (offset == 0) {
        // Slot status register.
        uint32_t slot_status_reg = 0;
        static_assert(BUS_MAX_DEVS <= UINT32_WIDTH,
                      "slot status register is too narrow");
        for (size_t slot = 0; slot < BUS_MAX_DEVS; slot++) {
            if (busctl->used_slots[slot]) { slot_status_reg |= (1 << slot); }
        }
        *out_val = slot_status_reg;

    } else if (offset >= 4 && (offset - 4) % 4 == 0) {
        // Slot X device descriptor.
        size_t slot = (offset - 4) / 12;
        if (slot < BUS_MAX_DEVS) {
            size_t item = ((offset - 4) % 12) / 4;
            if (item == 0) {
                // Mapped region start.
                *out_val = busctl->devs[slot].mmio.start;
            } else if (item == 1) {
                // Mapped region end.
                *out_val = busctl->devs[slot].mmio.end;
            } else if (item == 2) {
                // Device class and IRQ line.
                *out_val = (busctl->devs[slot].dev_class << 8) |
                           busctl->devs[slot].irq_line;
            }
        } else {
            err = VM_ERR_MEM_BAD_OP;
        }

    } else {
        err = VM_ERR_MEM_BAD_OP;
    }

    if (err != VM_ERR_NONE) {
        D_PRINTF("busctl MMIO: bad access at offset 0x%08X\n", offset);
    }
    return err;
}
