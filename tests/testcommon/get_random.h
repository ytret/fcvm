#pragma once

#include <cstdint>
#include <random>

#include <fcvm/cpu.h>

struct RegRef {
    enum Size {
        Size32Bits,
        Size8Bits,
    };
    Size size;
    uint8_t code;

    uint8_t encode() const {
        switch (size) {
        case Size32Bits:
            return CPU_REG_REF_SIZE_32 | code;
        case Size8Bits:
            return CPU_REG_REF_SIZE_8 | code;
        default:
            abort();
        }
    }

    uint32_t *get_ptr_u32(cpu_ctx_t *cpu) const {
        uint8_t encoded_ref = encode();
        cpu_reg_ref_t decoded_ref;
        cpu_decode_reg(cpu, encoded_ref, &decoded_ref);
        if (!decoded_ref.p_reg) {
            fprintf(stderr, "failed to decode register code 0x%02X\n", code);
            abort();
        }
        return decoded_ref.p_reg;
    }

    uint8_t *get_ptr_u8(cpu_ctx_t *cpu) const {
        uint8_t encoded_ref = encode();
        cpu_reg_ref_t decoded_ref;
        cpu_decode_reg(cpu, encoded_ref, &decoded_ref);
        if (!decoded_ref.p_reg) {
            fprintf(stderr, "failed to decode register code 0x%02X\n", code);
            abort();
        }
        return decoded_ref.p_reg_u8;
    }
};

vm_addr_t get_random_base_addr(std::mt19937 &rng);
vm_addr_t get_random_data_addr(std::mt19937 &rng, vm_addr_t data_start,
                               vm_addr_t data_end, size_t min_bytes_left);

uint8_t get_random_opcode(std::mt19937 &rng);

uint32_t *get_reg_ptr(cpu_ctx_t *cpu, uint8_t reg_code);
uint8_t get_random_reg_code(std::mt19937 &rng, bool unique_regs = false,
                            std::vector<uint8_t> used_reg_codes = {});
uint8_t get_random_reg_codes(std::mt19937 &rng, bool unique_regs = {},
                             std::vector<uint8_t> used_reg_codes = {});
RegRef get_random_reg_ref(std::mt19937 &rng, bool unique_regs = {},
                          std::vector<uint8_t> used_reg_codes = {});

uint32_t get_random_imm32(std::mt19937 &rng, uint32_t min_val = 0,
                          uint32_t max_val = UINT32_MAX);
bool get_random_bool(std::mt19937 &rng);
