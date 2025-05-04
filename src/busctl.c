#include <stdlib.h>
#include <string.h>

#include "busctl.h"
#include "debugm.h"

static bool prv_busctl_find_free_slot(busctl_ctx_t *busctl, size_t *out_idx);

static vm_err_t prv_busctl_mmio_read_u32(void *ctx, vm_addr_t addr,
                                         uint32_t *out_val);

busctl_ctx_t *busctl_new(memctl_ctx_t *memctl, intctl_ctx_t *intctl) {
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
    vm_err_t err = memctl_map_region(memctl, &busctl->bus_mmio);
    D_ASSERTMF(err.type == VM_ERR_NONE,
               "failed to map the bus MMIO, error type: %u", err.type);

    return busctl;
}

void busctl_free(busctl_ctx_t *busctl) {
    D_ASSERT(busctl);
    free(busctl);
}

vm_err_t busctl_connect_dev(busctl_ctx_t *busctl, const dev_desc_t *desc,
                            void *ctx, const busctl_dev_ctx_t **out_dev_ctx) {
    D_ASSERT(busctl);
    D_ASSERT(desc);
    D_ASSERT(out_dev_ctx);
    vm_err_t err = {.type = VM_ERR_NONE};

    // Find a free device slot.
    size_t slot;
    if (!prv_busctl_find_free_slot(busctl, &slot)) {
        err.type = VM_ERR_BUS_NO_FREE_SLOT;
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
        err.type = VM_ERR_BUS_NO_FREE_MEM;
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
    if (err.type != VM_ERR_NONE) { return err; }

    // Lock the slot and fill the device context.
    busctl->used_slots[slot] = true;
    busctl_dev_ctx_t *dev_ctx = &busctl->devs[slot];
    memset(dev_ctx, 0, sizeof(*dev_ctx));
    dev_ctx->bus_slot = slot;
    dev_ctx->dev_class = desc->dev_class;
    dev_ctx->irq_line = irq_line;
    dev_ctx->mmio = mmio;
    *out_dev_ctx = dev_ctx;

    D_ASSERT(err.type == VM_ERR_NONE);
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

    vm_err_t err = {.type = VM_ERR_NONE};

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
            err.type = VM_ERR_MEM_BAD_OP;
        }

    } else {
        err.type = VM_ERR_MEM_BAD_OP;
    }

    if (err.type != VM_ERR_NONE) {
        D_PRINTF("busctl MMIO: bad access at offset 0x%08X\n", offset);
    }
    return err;
}
