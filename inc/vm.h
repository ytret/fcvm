#pragma once

#include "cpu.h"
#include "memctl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    memctl_ctx_t *memctl;
    cpu_ctx_t *cpu;
} vm_ctx_t;

vm_ctx_t *vm_new(memctl_ctx_t *memctl, cpu_ctx_t *cpu);
void vm_free(vm_ctx_t *vm);

void vm_step(vm_ctx_t *vm);

#ifdef __cplusplus
}
#endif
