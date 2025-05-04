#pragma once

#include "cpu.h"
#include "memctl.h"
#include "busctl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VM_MAX_DEV_REGS 32

typedef struct {
    memctl_ctx_t *memctl;
    cpu_ctx_t *cpu;
    busctl_ctx_t *busctl;

    dev_desc_t dev_regs[VM_MAX_DEV_REGS];
    size_t num_dev_regs;
} vm_ctx_t;

vm_ctx_t *vm_new(void);
void vm_free(vm_ctx_t *vm);

vm_err_t vm_reg_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc);
vm_err_t vm_connect_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc, void *ctx);
void vm_step(vm_ctx_t *vm);

#ifdef __cplusplus
}
#endif
