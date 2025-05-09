/**
 * \file cpu_stack.h
 * CPU stack operations API
 */

#include <fcvm/cpu.h>

vm_err_t cpu_stack_push_u32(cpu_ctx_t *cpu, uint32_t val);
vm_err_t cpu_stack_pop_u32(cpu_ctx_t *cpu, uint32_t *out_val);
