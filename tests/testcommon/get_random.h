#pragma once

#include <random>
#include <cstdint>

#include <fcvm/cpu.h>

vm_addr_t get_random_base_addr(std::mt19937 &rng);
vm_addr_t get_random_data_addr(std::mt19937 &rng, vm_addr_t data_start,
                               vm_addr_t data_end, size_t min_bytes_left);

uint8_t get_random_opcode(std::mt19937 &rng);

uint32_t *get_reg_ptr(cpu_ctx_t *cpu, uint8_t reg_code);
uint8_t get_random_reg_code(std::mt19937 &rng, bool unique_regs = false,
                            std::vector<uint8_t> used_reg_codes = {});
uint8_t get_random_reg_codes(std::mt19937 &rng, bool unique_regs = {},
                             std::vector<uint8_t> used_reg_codes = {});

uint32_t get_random_imm32(std::mt19937 &rng, uint32_t min_val = 0,
                          uint32_t max_val = UINT32_MAX);
bool get_random_bool(std::mt19937 &rng);
