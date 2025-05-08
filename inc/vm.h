#pragma once

#include "busctl.h"
#include "cpu.h"
#include "memctl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VM_MAX_DEV_REGS 32

/// Version of the `vm_ctx_t` structure and its member structures.
/// Increment this every time anything in the `vm_ctx_t` structure or its member
/// structures is changed: field order, size, type, etc.
#define SN_VM_CTX_VER ((uint32_t)1)

typedef struct {
    memctl_ctx_t *memctl;
    cpu_ctx_t *cpu;
    busctl_ctx_t *busctl;
} vm_ctx_t;

vm_ctx_t *vm_new(void);
void vm_free(vm_ctx_t *vm);
size_t vm_snapshot_size(const vm_ctx_t *vm);
size_t vm_snapshot(const vm_ctx_t *vm, void *v_buf, size_t max_size);
vm_ctx_t *vm_restore(cb_restore_dev_t f_restore_dev, const void *v_buf,
                     size_t max_size, size_t *out_size);

vm_err_t vm_connect_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc, void *ctx);
void vm_step(vm_ctx_t *vm);

#ifdef __cplusplus
}
#endif
