#pragma once

#include <stdint.h>

#include "vm.h"

uint8_t mem_read_u8(vm_state_t *vm, uint32_t addr);
void mem_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte);
uint32_t mem_read_u32(vm_state_t *vm, uint32_t addr);
void mem_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword);
