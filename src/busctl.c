#include <stdlib.h>
#include <string.h>

#include "busctl.h"
#include "debugm.h"

static bool prv_busctl_find_free_slot(busctl_ctx_t *busctl, size_t *out_idx);

busctl_ctx_t *busctl_new(mem_ctx_t *mem, intctl_ctx_t *intctl) {
    D_ASSERT(mem);
    D_ASSERT(intctl);

    busctl_ctx_t *busctl = malloc(sizeof(*busctl));
    D_ASSERT(busctl);
    memset(busctl, 0, sizeof(*busctl));

    busctl->memctl = mem;
    busctl->intctl = intctl;
    busctl->next_region_at = BUS_DEV_MAP_START;
    busctl->next_irq_line = 0;

    return busctl;
}

void busctl_free(busctl_ctx_t *busctl) {
    D_ASSERT(busctl);
    free(busctl);
}

vm_err_t busctl_reg_dev(busctl_ctx_t *busctl, const busctl_req_t *req,
                        const busctl_dev_ctx_t **out_dev_ctx) {
    D_ASSERT(busctl);
    D_ASSERT(req);
    D_ASSERT(out_dev_ctx);
    vm_err_t err = {.type = VM_ERR_NONE};

    // Find a free device slot.
    size_t slot;
    if (!prv_busctl_find_free_slot(busctl, &slot)) {
        err.type = VM_ERR_BUS_NO_FREE_SLOT;
        return err;
    }

    // Allocate resources for the device, but don't commit them yet. Commitment
    // is done at the end of this function if the registration is successful.
    uint8_t irq_line = busctl->next_irq_line;
    static_assert(BUS_MAX_DEVS < UINT8_MAX,
                  "cannot support this much devices because of the IRQ limit");
    vm_addr_t map_start = busctl->next_region_at;
    vm_addr_t map_end = map_start + req->region_size;
    if (map_end >= BUS_DEV_MAP_END) {
        err.type = VM_ERR_BUS_NO_FREE_MEM;
        return err;
    }

    // Map the region.
    mmio_region_t mmio = {
        .start = map_start,
        .end = map_end,
        .ctx = req->ctx,
        .mem_if = req->mem_if,
    };
    err = mem_map_region(busctl->memctl, &mmio);
    if (err.type != VM_ERR_NONE) { return err; }

    // Lock the slot and fill the device context.
    busctl->used_slots[slot] = true;
    busctl_dev_ctx_t *dev_ctx = &busctl->devs[slot];
    memset(dev_ctx, 0, sizeof(*dev_ctx));
    dev_ctx->dev_class = req->dev_class;
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
