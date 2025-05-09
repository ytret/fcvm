#pragma once

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

    VM_ERR_INVALID_IRQ_NUM,

    VM_ERR_BUS_NO_FREE_SLOT,
    VM_ERR_BUS_NO_FREE_MEM,

    VM_ERR_MEM_MAX_REGIONS,
    VM_ERR_MEM_USED,
    VM_ERR_MEM_BAD_OP,

    VM_ERR_MAX_DEV_REGS,
} vm_err_type_t;

typedef struct {
    vm_err_type_t type;
} vm_err_t;

#ifdef __cplusplus
}
#endif
