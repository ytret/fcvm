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

/**
 * Returns one of the pending IRQs.
 * Resets the pending state of the returned IRQ.
 *
 * \param[in] intctl -- Interrupt controller context.
 * \param[out] out_irq -- Pending IRQ.
 *
 * \returns `true` if a pending IRQ was written to \a out_irq and reset, `false`
 * if there is no pending IRQ.
 */
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
