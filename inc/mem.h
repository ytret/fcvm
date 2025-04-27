#pragma once

#include <stdint.h>

#include "exc.h"
#include "vm.h"

vm_res_u8_t mem_read_u8(vm_state_t *vm, uint32_t addr);
vm_res_t mem_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte);
vm_res_u32_t mem_read_u32(vm_state_t *vm, uint32_t addr);
vm_res_t mem_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword);
