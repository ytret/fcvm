#pragma once

#include "cpu.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mem_ctx_t *mem;
    cpu_ctx_t *cpu;
} vm_ctx_t;

vm_ctx_t *vm_new(mem_ctx_t *mem, cpu_ctx_t *cpu);
void vm_free(vm_ctx_t *vm);

void vm_step(vm_ctx_t *vm);

#ifdef __cplusplus
}
#endif
