#pragma once

#include <stdint.h>

#include "vm_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VM_RES_NONE,
    VM_RES_U8,
    VM_RES_U16,
    VM_RES_U32,
    VM_RES_ERR,
} vm_res_type_t;

typedef struct {
    vm_res_type_t type;
    union {
        uint8_t byte;
        uint16_t word;
        uint32_t dword;
        vm_err_type_t err;
    };
} vm_res_t;

#ifdef __cplusplus
}
#endif
