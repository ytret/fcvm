#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "vm.h"

static bool prv_vm_find_dev_desc(const vm_ctx_t *vm, uint8_t dev_class,
                                 const dev_desc_t **out_dev_desc);

vm_ctx_t *vm_new(void) {
    vm_ctx_t *vm = malloc(sizeof(*vm));
    D_ASSERT(vm);
    memset(vm, 0, sizeof(*vm));

    vm->memctl = memctl_new();
    vm->cpu = cpu_new(&vm->memctl->intf);
    vm->busctl = busctl_new(vm->memctl, vm->cpu->intctl);

    vm->num_dev_regs = 0;

    return vm;
}

void vm_free(vm_ctx_t *vm) {
    D_ASSERT(vm);
    busctl_free(vm->busctl);
    cpu_free(vm->cpu);
    memctl_free(vm->memctl);
    free(vm);
}

vm_err_t vm_reg_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc) {
    D_ASSERT(vm);
    D_ASSERT(dev_desc);
    vm_err_t err = {.type = VM_ERR_NONE};

    if (vm->num_dev_regs >= VM_MAX_DEV_REGS) {
        err.type = VM_ERR_MAX_DEV_REGS;
        return err;
    } else if (prv_vm_find_dev_desc(vm, dev_desc->dev_class, NULL)) {
        err.type = VM_ERR_DEV_CLASS_EXISTS;
        return err;
    }

    memcpy(&vm->dev_regs[vm->num_dev_regs], dev_desc, sizeof(dev_desc_t));
    vm->num_dev_regs++;

    D_PRINTF("registered a device class 0x%02X", dev_desc->dev_class);
    return err;
}

vm_err_t vm_connect_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc, void *ctx) {
    D_ASSERT(vm);
    D_ASSERT(dev_desc);
    D_ASSERT(ctx);
    vm_err_t err = {.type = VM_ERR_NONE};

    if (prv_vm_find_dev_desc(vm, dev_desc->dev_class, NULL)) {
        err = busctl_connect_dev(vm->busctl, dev_desc, ctx, NULL);
    } else {
        err.type = VM_ERR_DEV_CLASS_UNKNOWN;
    }

    return err;
}

void vm_step(vm_ctx_t *vm) {
    D_ASSERT(vm);
    D_ASSERT(vm->cpu);
    cpu_step(vm->cpu);
}

static bool prv_vm_find_dev_desc(const vm_ctx_t *vm, uint8_t dev_class,
                                 const dev_desc_t **out_dev_desc) {
    D_ASSERT(vm);
    for (size_t idx = 0; idx < vm->num_dev_regs; idx++) {
        const dev_desc_t *dev_desc = &vm->dev_regs[idx];
        if (dev_desc->dev_class == dev_class) {
            if (out_dev_desc) { *out_dev_desc = dev_desc; }
            return true;
        }
    }
    return false;
}
