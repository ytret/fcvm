/**
 * \file intctl.h
 * CPU interrupt controller.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "vm_err.h"

#define INTCTL_MAX_IRQ_NUM 31

typedef struct {
    uint32_t raised_irqs;
} intctl_ctx_t;

intctl_ctx_t *intctl_new(void);
void intctl_free(intctl_ctx_t *intctl);

bool intctl_has_pending_irqs(intctl_ctx_t *intctl);

/**
 * Sets the pending state of an IRQ line \a irq_line.
 * \param intctl -- Interrupt controller.
 * \param irq_line -- IRQ line to be raised (must be less than or equal to
 *                    #INTCTL_MAX_IRQ_NUM).
 * \returns #VM_ERR_NONE if the IRQ was raised, #VM_ERR_INVALID_IRQ_NUM if the
 * IRQ num is greater than #INTCTL_MAX_IRQ_NUM.
 */
vm_err_t intctl_raise_irq_line(intctl_ctx_t *intctl, uint8_t irq_line);

/**
 * Returns one of the pending IRQs.
 * Resets the pending state of the returned IRQ.
 *
 * \param[in] intctl -- Interrupt controller.
 * \param[out] out_irq -- Pending IRQ.
 *
 * \returns `true` if a pending IRQ was written to \a out_irq and reset, `false`
 * if there is no pending IRQ.
 */
bool intctl_get_pending_irq(intctl_ctx_t *intctl, uint8_t *out_irq);

#ifdef __cplusplus
}
#endif
