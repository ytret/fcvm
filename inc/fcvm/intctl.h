/**
 * @file intctl.h
 * Interrupt controller API.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <fcvm/vm_err.h>

/// Version of the `intctl_ctx_t` structure and its member structures.
/// Increment this every time anything in the `intctl_ctx_t` structure or its
/// member structures is changed: field order, size, type, etc.
#define SN_INTCTL_CTX_VER ((uint32_t)1)

#define INTCTL_MAX_IRQ_NUM 31

typedef struct {
    uint32_t raised_irqs;
} intctl_ctx_t;

intctl_ctx_t *intctl_new(void);
void intctl_free(intctl_ctx_t *intctl);

/// @addtogroup snapshots
/// @{

/// Calculates the size of a buffer required to store an #intctl_ctx_t snapshot.
size_t intctl_snapshot_size(void);
/**
 * Writes a snapshot of @a intctl into the buffer @a v_buf.
 * @param intctl   Interrupt controller context to save a snapshot of.
 * @param v_buf    Snapshot buffer.
 * @param max_size Size of @a v_buf.
 * @returns Size of the saved snapshot in bytes.
 * @note @a max_size should be more than or equal to the size returned by
 * #intctl_snapshot_size().
 */
size_t intctl_snapshot(const intctl_ctx_t *intctl, void *v_buf,
                       size_t max_size);
/**
 * Restores an #intctl_ctx_t structure from a snapshot buffer.
 * @param      v_buf         Snapshot buffer.
 * @param      max_size      Size of @a v_buf.
 * @param[out] out_used_size Number of bytes used from the buffer @a v_buf.
 * @returns A newly created interrupt controller context restored from the
 * buffer @a v_buf.
 */
intctl_ctx_t *intctl_restore(const void *v_buf, size_t max_size,
                             size_t *out_used_size);
/// @}

bool intctl_has_pending_irqs(intctl_ctx_t *intctl);

/**
 * Sets the pending state of an IRQ line @a irq_line.
 * @param intctl   Interrupt controller.
 * @param irq_line IRQ line to be raised (must be less than or equal to
 *                 #INTCTL_MAX_IRQ_NUM).
 * @returns #VM_ERR_NONE if the IRQ was raised, #VM_ERR_INVALID_IRQ_NUM if the
 * IRQ num is greater than #INTCTL_MAX_IRQ_NUM.
 */
vm_err_t intctl_raise_irq_line(intctl_ctx_t *intctl, uint8_t irq_line);

/**
 * Returns one of the pending IRQs.
 * Resets the pending state of the returned IRQ.
 *
 * @param[in]  intctl  Interrupt controller.
 * @param[out] out_irq Pending IRQ.
 *
 * @returns `true` if a pending IRQ was written to @a out_irq and reset, `false`
 * if there is no pending IRQ.
 */
bool intctl_get_pending_irq(intctl_ctx_t *intctl, uint8_t *out_irq);

#ifdef __cplusplus
}
#endif
