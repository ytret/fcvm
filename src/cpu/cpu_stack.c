/**
 * @file cpu_stack.c
 * CPU stack operations implementation
 */

#include "cpu_stack.h"
#include "debugm.h"

vm_err_t cpu_stack_push_u32(cpu_ctx_t *cpu, uint32_t val) {
    D_ASSERT(cpu != NULL);
    if (cpu->reg_sp >= 4) {
        cpu->reg_sp -= 4;
        return cpu->mem->write_u32(cpu->mem, cpu->reg_sp, val);
    } else {
        vm_err_t err = VM_ERR_STACK_OVERFLOW;
        return err;
    }
}

vm_err_t cpu_stack_pop_u32(cpu_ctx_t *cpu, uint32_t *out_val) {
    D_ASSERT(cpu != NULL);
    vm_err_t err = cpu->mem->read_u32(cpu->mem, cpu->reg_sp, out_val);
    if (err == VM_ERR_NONE) {
        if (cpu->reg_sp <= 0xFFFFFFFF - 4) {
            cpu->reg_sp += 4;
        } else {
            err = VM_ERR_STACK_OVERFLOW;
        }
    }
    return err;
}
