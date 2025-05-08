#include <stdbit.h>
#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "intctl.h"

intctl_ctx_t *intctl_new(void) {
    intctl_ctx_t *intctl = malloc(sizeof(*intctl));
    D_ASSERT(intctl);
    memset(intctl, 0, sizeof(*intctl));

    return intctl;
}

void intctl_free(intctl_ctx_t *intctl) {
    D_ASSERT(intctl);
    free(intctl);
}

size_t intctl_snapshot_size(void) {
    static_assert(SN_INTCTL_CTX_VER == 1);
    return sizeof(intctl_ctx_t);
}

size_t intctl_snapshot(const intctl_ctx_t *intctl, void *v_buf,
                       size_t max_size) {
    static_assert(SN_INTCTL_CTX_VER == 1);
    D_ASSERT(intctl);
    D_ASSERT(v_buf);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t size = 0;

    // Write the intctl context.
    D_ASSERT(size + sizeof(*intctl) <= max_size);
    memcpy(&buf[size], intctl, sizeof(*intctl));
    size += sizeof(*intctl);

    return size;
}

intctl_ctx_t *intctl_restore(const void *v_buf, size_t max_size,
                             size_t *out_used_size) {
    static_assert(SN_INTCTL_CTX_VER == 1);
    D_ASSERT(v_buf);
    D_ASSERT(out_used_size);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t offset = 0;

    // Restore the intctl context.
    intctl_ctx_t rest_intctl;
    D_ASSERT(offset + sizeof(rest_intctl) <= max_size);
    memcpy(&rest_intctl, &buf[offset], sizeof(rest_intctl));
    offset += sizeof(rest_intctl);

    // Create a new intctl and set the fields manually.
    intctl_ctx_t *intctl = intctl_new();
    intctl->raised_irqs = rest_intctl.raised_irqs;

    *out_used_size = offset;
    return intctl;
}

bool intctl_has_pending_irqs(intctl_ctx_t *intctl) {
    D_ASSERT(intctl);
    return intctl->raised_irqs != 0;
}

vm_err_t intctl_raise_irq_line(intctl_ctx_t *intctl, uint8_t irq_line) {
    D_ASSERT(intctl);
    vm_err_t err = {.type = VM_ERR_NONE};
    if (irq_line <= INTCTL_MAX_IRQ_NUM) {
        intctl->raised_irqs |= (1 << irq_line);
    } else {
        err.type = VM_ERR_INVALID_IRQ_NUM;
    }
    return err;
}

bool intctl_get_pending_irq(intctl_ctx_t *intctl, uint8_t *out_irq) {
    D_ASSERT(intctl);
    if (intctl->raised_irqs) {
        int fto = stdc_first_trailing_one(intctl->raised_irqs);
        D_ASSERT(fto != 0);
        uint8_t irq_num = fto - 1;
        intctl->raised_irqs &= ~(1 << irq_num);
        *out_irq = irq_num;
        return true;
    } else {
        return false;
    }
}
