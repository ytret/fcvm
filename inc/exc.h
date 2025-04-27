#pragma once

#include <stdint.h>

typedef enum {
    VM_EXC_NONE,
    VM_EXC_MEM_FAULT,
    VM_EXC_STACK_OVERFLOW,
    VM_EXC_BAD_OPCODE,
    VM_EXC_DIVIDE_BY_ZERO,
} vm_exc_type_t;

typedef struct {
    vm_exc_type_t type;
} vm_exc_t;

typedef struct {
    bool ok;
    uint8_t byte;
    vm_exc_t exc;
} vm_res_u8_t;

typedef struct {
    bool ok;
    uint32_t dword;
    vm_exc_t exc;
} vm_res_u32_t;

typedef struct {
    bool ok;
    uint32_t *ptr;
    vm_exc_t exc;
} vm_res_pu32_t;

typedef struct {
    bool ok;
    vm_exc_t exc;
} vm_res_t;
