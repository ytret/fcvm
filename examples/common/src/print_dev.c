#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ex_common/print_dev.h>

#define PRINT_DEV_REG_CTRL   offsetof(print_dev_regs_t, ctrl)
#define PRINT_DEV_REG_OUTBUF offsetof(print_dev_regs_t, outbuf)

static void prv_print_dev_flush(print_dev_ctx_t *ctx);

print_dev_ctx_t *print_dev_new(void) {
    print_dev_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) { return NULL; }
    memset(ctx, 0, sizeof(*ctx));

    ctx->desc.dev_class = PRINT_DEV_CLASS;
    ctx->desc.region_size = sizeof(ctx->regs);
    ctx->desc.mem_if.read_u8 = NULL; // reads are not supported
    ctx->desc.mem_if.read_u32 = NULL;
    ctx->desc.mem_if.write_u8 = print_dev_write_u8;
    ctx->desc.mem_if.write_u32 = print_dev_write_u32;
    ctx->desc.f_snapshot_size = print_dev_snapshot_size;
    ctx->desc.f_snapshot = print_dev_snapshot;

    return ctx;
}

size_t print_dev_snapshot_size(const void *v_ctx) {
    (void)v_ctx;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

size_t print_dev_snapshot(const void *v_ctx, void *v_buf, size_t max_size) {
    (void)v_ctx;
    (void)v_buf;
    (void)max_size;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

print_dev_ctx_t *print_dev_restore(const void *v_buf, size_t max_size) {
    (void)v_buf;
    (void)max_size;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

vm_err_t print_dev_write_u8(void *v_ctx, vm_addr_t addr, uint8_t val) {
    print_dev_ctx_t *ctx = v_ctx;

    if (addr < PRINT_DEV_REG_OUTBUF) {
        // 8-bit writes are allowed only into the outbuf register.
        return VM_ERR_BAD_MEM;
    }

    const vm_addr_t offset = addr - PRINT_DEV_REG_OUTBUF;
    assert((offset + 1) <= PRINT_DEV_BUF_SIZE);
    ctx->regs.outbuf[offset] = val;
    return VM_ERR_NONE;
}

vm_err_t print_dev_write_u32(void *v_ctx, vm_addr_t addr, uint32_t val) {
    print_dev_ctx_t *ctx = v_ctx;
    vm_err_t err = VM_ERR_NONE;

    if (addr < PRINT_DEV_REG_CTRL + sizeof(ctx->regs.ctrl)) {
        if (addr == PRINT_DEV_REG_CTRL) {
            ctx->regs.ctrl = val;
        } else {
            fprintf(stderr, "print_dev: unaligned write at 0x%08X\n", addr);
            err = VM_ERR_BAD_MEM;
        }
    } else {
        const vm_addr_t offset = addr - PRINT_DEV_REG_OUTBUF;
        assert((offset + 4) <= PRINT_DEV_BUF_SIZE);
        memcpy(&ctx->regs.outbuf[offset], &val, 4);
    }

    if (ctx->regs.ctrl_bits.flush) {
        prv_print_dev_flush(ctx);
        ctx->regs.ctrl_bits.flush = false;
    }

    return err;
}

static void prv_print_dev_flush(print_dev_ctx_t *ctx) {
    const size_t len =
        strnlen((const char *)ctx->regs.outbuf, PRINT_DEV_BUF_SIZE);

    if (len < PRINT_DEV_BUF_SIZE) {
        printf("%s\n", (const char *)ctx->regs.outbuf);
    } else {
        fprintf(stderr,
                "print_dev: output string is not NUL-terminated, not printing");
    }
}
