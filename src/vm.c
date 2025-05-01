#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "vm.h"

vm_ctx_t *vm_new(mem_ctx_t *mem, cpu_ctx_t *cpu) {
    D_ASSERT(mem);
    D_ASSERT(cpu);

    vm_ctx_t *vm = malloc(sizeof(*vm));
    D_ASSERT(vm);
    memset(vm, 0, sizeof(*vm));

    vm->mem = mem;
    vm->cpu = cpu;

    return vm;
}

void vm_free(vm_ctx_t *vm) {
    D_ASSERT(vm);
    free(vm);
}

void vm_step(vm_ctx_t *vm) {
    D_ASSERT(vm);
    D_ASSERT(vm->cpu);
}
