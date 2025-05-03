#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "vm.h"

vm_ctx_t *vm_new(memctl_ctx_t *memctl, cpu_ctx_t *cpu) {
    D_ASSERT(memctl);
    D_ASSERT(cpu);

    vm_ctx_t *vm = malloc(sizeof(*vm));
    D_ASSERT(vm);
    memset(vm, 0, sizeof(*vm));

    vm->memctl = memctl;
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
