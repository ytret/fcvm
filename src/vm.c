#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "vm.h"

vm_ctx_t *vm_new(void) {
    vm_ctx_t *vm = malloc(sizeof(*vm));
    D_ASSERT(vm);
    memset(vm, 0, sizeof(*vm));

    vm->memctl = memctl_new();
    vm->cpu = cpu_new(&vm->memctl->intf);
    vm->busctl = busctl_new(vm->memctl, vm->cpu->intctl);

    return vm;
}

void vm_free(vm_ctx_t *vm) {
    D_ASSERT(vm);
    busctl_free(vm->busctl);
    cpu_free(vm->cpu);
    memctl_free(vm->memctl);
    free(vm);
}

size_t vm_snapshot_size(const vm_ctx_t *vm) {
    static_assert(SN_VM_CTX_VER == 1);
    return sizeof(vm_ctx_t) + memctl_snapshot_size() + cpu_snapshot_size() +
           busctl_snapshot_size(vm->busctl);
}

size_t vm_snapshot(const vm_ctx_t *vm, void *v_buf, size_t max_size) {
    static_assert(SN_VM_CTX_VER == 1);
    D_ASSERT(vm);
    D_ASSERT(v_buf);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t size = 0;

    // Replace every pointer by NULL.
    vm_ctx_t vm_copy;
    memcpy(&vm_copy, vm, sizeof(vm_copy));
    vm_copy.memctl = NULL;
    vm_copy.cpu = NULL;
    vm_copy.busctl = NULL;

    // Write the VM context.
    D_ASSERT(size + sizeof(vm_copy) <= max_size);
    memcpy(&buf[size], &vm_copy, sizeof(vm_copy));
    size += sizeof(vm_copy);

    // Save the vm->memctl context.
    size += memctl_snapshot(vm->memctl, &buf[size], max_size - size);

    // Save the vm->cpu context.
    size += cpu_snapshot(vm->cpu, &buf[size], max_size - size);

    // Save the vm->busctl context.
    size += busctl_snapshot(vm->busctl, &buf[size], max_size - size);

    return size;
}

vm_ctx_t *vm_restore(cb_restore_dev_t f_restore_dev, const void *v_buf,
                     size_t max_size, size_t *out_used_size) {
    static_assert(SN_VM_CTX_VER == 1);
    D_ASSERT(f_restore_dev);
    D_ASSERT(v_buf);
    D_ASSERT(out_used_size);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t offset = 0;

    vm_ctx_t *vm = malloc(sizeof(*vm));
    D_ASSERT(vm);
    memset(vm, 0, sizeof(*vm));

    size_t memctl_size = 0;
    vm->memctl = memctl_restore(&buf[offset], max_size - offset, &memctl_size);
    offset += memctl_size;

    size_t cpu_size = 0;
    vm->cpu = cpu_restore(&vm->memctl->intf, &buf[offset], max_size - offset,
                          &cpu_size);
    offset += cpu_size;

    size_t busctl_size = 0;
    vm->busctl = busctl_restore(vm->memctl, vm->cpu->intctl, f_restore_dev,
                                &buf[offset], max_size - offset, &busctl_size);
    offset += busctl_size;

    *out_used_size = offset;
    return vm;
}

vm_err_t vm_connect_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc, void *ctx) {
    D_ASSERT(vm);
    D_ASSERT(dev_desc);
    D_ASSERT(ctx);
    return busctl_connect_dev(vm->busctl, dev_desc, ctx, NULL);
}

void vm_step(vm_ctx_t *vm) {
    D_ASSERT(vm);
    D_ASSERT(vm->cpu);
    cpu_step(vm->cpu);
}
