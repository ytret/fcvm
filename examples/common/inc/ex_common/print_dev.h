/**
 * @file print_dev.h
 * Example synchronous stdout device API.
 *
 * A write-only memory-mapped device for printing stuff to the standard output
 * of the VM process. Guest program may access the MMIO region using bytes or
 * dwords.
 *
 * It provides two memory-mapped registers to the guest program:
 * - @ref print_dev_regs_t.ctrl "Control register".
 * - @ref print_dev_regs_t.outbuf "Buffer register".
 *
 * Using the device:
 * 1. Fill the output buffer with bytes to print to the stdout.
 * 2. Set `ctrl.flush` to 1.
 */

#pragma once

#include <fcvm/vm_types.h>

#define PRINT_DEV_CLASS    0x01
#define PRINT_DEV_BUF_SIZE 128

typedef struct [[gnu::packed]] {
    /// Control register.
    union {
        uint32_t ctrl;
        struct {
            /**
             * Flush bit.
             * When set to 1 by software, the device synchronously flushes the
             * buffer to stdout and resets the bit to 0.
             */
            bool flush : 1;

            uint32_t unused : 31; //!< Unused bits.
        } ctrl_bits;
    };

    /// Output buffer.
    uint8_t outbuf[PRINT_DEV_BUF_SIZE];
} print_dev_regs_t;

static_assert(sizeof(((print_dev_regs_t *)0)->ctrl) == 4);

/// Print device context.
typedef struct {
    print_dev_regs_t regs;

    dev_desc_t desc;
} print_dev_ctx_t;

print_dev_ctx_t *print_dev_new(void);

size_t print_dev_snapshot_size(const void *v_ctx);
size_t print_dev_snapshot(const void *v_ctx, void *v_buf, size_t max_size);
print_dev_ctx_t *print_dev_restore(const void *v_buf, size_t max_size);

vm_err_t print_dev_write_u8(void *v_ctx, vm_addr_t addr, uint8_t val);
vm_err_t print_dev_write_u32(void *v_ctx, vm_addr_t addr, uint32_t val);
