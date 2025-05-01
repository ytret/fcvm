#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VM_ERR_NONE = 0, // Must be zero, used in `if` statements.
    VM_ERR_BAD_MEM,

    VM_ERR_BAD_OPCODE,
    VM_ERR_BAD_REG_CODE,
    VM_ERR_BAD_IMM5,

    VM_ERR_DIV_BY_ZERO,
    VM_ERR_STACK_OVERFLOW,
} vm_err_type_t;

typedef struct {
    vm_err_type_t type;
} vm_err_t;

uint8_t vm_err_to_irq_line(vm_err_t err);

#ifdef __cplusplus
}
#endif
