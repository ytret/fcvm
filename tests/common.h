#pragma once

#include <algorithm>
#include <array>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"

inline uint32_t *get_reg_ptr(cpu_ctx_t *cpu, uint8_t reg_code) {
    uint32_t *p_reg;
    cpu_decode_reg(cpu, reg_code, &p_reg);
    if (!p_reg) {
        fprintf(stderr, "failed to decode register code 0x%02X\n", reg_code);
        abort();
    }
    return p_reg;
}

inline vm_addr_t get_random_base_addr(std::mt19937 &rng) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(0, VM_MAX_ADDR);
    return addr_dist(rng);
}

inline vm_addr_t get_random_data_addr(std::mt19937 &rng, vm_addr_t data_start,
                                      vm_addr_t data_end,
                                      size_t min_bytes_left) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(
        data_start, data_end - min_bytes_left);
    return addr_dist(rng);
}

inline uint8_t get_random_reg_code(std::mt19937 &rng, bool unique_regs = false,
                                   std::vector<uint8_t> used_reg_codes = {}) {
    std::array<uint8_t, 9> reg_codes = {
        CPU_CODE_R0, CPU_CODE_R1, CPU_CODE_R2, CPU_CODE_R3, CPU_CODE_R4,
        CPU_CODE_R5, CPU_CODE_R6, CPU_CODE_R7, CPU_CODE_SP,
    };
    static_assert(CPU_NUM_GP_REG_CODES == 8, "update test code");
    std::uniform_int_distribution<size_t> reg_dist(0, reg_codes.size() - 1);
    uint8_t reg_code;
    do {
        reg_code = reg_codes[reg_dist(rng)];
    } while (unique_regs &&
             std::find(used_reg_codes.cbegin(), used_reg_codes.cend(),
                       reg_code) != used_reg_codes.cend());
    return reg_code;
}

inline uint8_t get_random_reg_codes(std::mt19937 &rng, bool unique_regs = {},
                                    std::vector<uint8_t> used_reg_codes = {}) {
    uint8_t reg_code1 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    used_reg_codes.push_back(reg_code1);
    uint8_t reg_code2 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    return (reg_code1 << 4) | reg_code2;
}

inline uint32_t get_random_imm32(std::mt19937 &rng) {
    std::uniform_int_distribution<uint32_t> val_dist(0, UINT32_MAX);
    return val_dist(rng);
}

inline bool get_random_bool(std::mt19937 &rng) {
    std::uniform_int_distribution<uint32_t> val_dist(0, 1);
    return val_dist(rng) == 0;
}
