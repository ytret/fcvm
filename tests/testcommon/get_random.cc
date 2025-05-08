#include <algorithm>
#include <array>

#include <stdio.h>
#include <stdlib.h>

#include "testcommon/get_random.h"

vm_addr_t get_random_base_addr(std::mt19937 &rng) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(0, VM_MAX_ADDR);
    return addr_dist(rng);
}

vm_addr_t get_random_data_addr(std::mt19937 &rng, vm_addr_t data_start,
                               vm_addr_t data_end, size_t min_bytes_left) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(
        data_start, data_end - min_bytes_left);
    return addr_dist(rng);
}

uint8_t get_random_opcode(std::mt19937 &rng) {
    constexpr std::array<uint8_t, 71> opcodes = {
        CPU_OP_MOV_VR,   CPU_OP_MOV_RR,    CPU_OP_STR_RV0,  CPU_OP_STR_RI0,
        CPU_OP_STR_RI8,  CPU_OP_STR_RI32,  CPU_OP_STR_RIR,  CPU_OP_LDR_RV0,
        CPU_OP_LDR_RI0,  CPU_OP_LDR_RI8,   CPU_OP_LDR_RI32, CPU_OP_LDR_RIR,
        CPU_OP_ADD_RR,   CPU_OP_ADD_RV,    CPU_OP_SUB_RR,   CPU_OP_SUB_RV,
        CPU_OP_MUL_RR,   CPU_OP_MUL_RV,    CPU_OP_DIV_RR,   CPU_OP_DIV_RV,
        CPU_OP_IDIV_RR,  CPU_OP_IDIV_RV,   CPU_OP_AND_RR,   CPU_OP_AND_RV,
        CPU_OP_OR_RR,    CPU_OP_OR_RV,     CPU_OP_XOR_RR,   CPU_OP_XOR_RV,
        CPU_OP_NOT_R,    CPU_OP_SHL_RR,    CPU_OP_SHL_RV,   CPU_OP_SHR_RR,
        CPU_OP_SHR_RV,   CPU_OP_ROR_RR,    CPU_OP_ROR_RV,   CPU_OP_ROL_RR,
        CPU_OP_ROL_RV,   CPU_OP_CMP_RR,    CPU_OP_TST_RR,   CPU_OP_TST_RV,
        CPU_OP_JMPR_V8,  CPU_OP_JMPA_V32,  CPU_OP_JMPA_R,   CPU_OP_JEQR_V8,
        CPU_OP_JEQA_V32, CPU_OP_JEQA_R,    CPU_OP_JNER_V8,  CPU_OP_JNEA_V32,
        CPU_OP_JNEA_R,   CPU_OP_JGTR_V8,   CPU_OP_JGTA_V32, CPU_OP_JGTA_R,
        CPU_OP_JGER_V8,  CPU_OP_JGEA_V32,  CPU_OP_JGEA_R,   CPU_OP_JLTR_V8,
        CPU_OP_JLTA_V32, CPU_OP_JLTA_R,    CPU_OP_JLER_V8,  CPU_OP_JLEA_V32,
        CPU_OP_JLEA_R,   CPU_OP_CALLA_V32, CPU_OP_CALLA_R,  CPU_OP_RET,
        CPU_OP_PUSH_V32, CPU_OP_PUSH_R,    CPU_OP_POP_R,    CPU_OP_NOP,
        CPU_OP_HALT,     CPU_OP_INT_V8,    CPU_OP_IRET,
    };
    std::uniform_int_distribution<uint8_t> opcode_dist(0, opcodes.size() - 1);
    return opcode_dist(rng);
}

uint32_t *get_reg_ptr(cpu_ctx_t *cpu, uint8_t reg_code) {
    uint32_t *p_reg;
    cpu_decode_reg(cpu, reg_code, &p_reg);
    if (!p_reg) {
        fprintf(stderr, "failed to decode register code 0x%02X\n", reg_code);
        abort();
    }
    return p_reg;
}

uint8_t get_random_reg_code(std::mt19937 &rng, bool unique_regs,
                            std::vector<uint8_t> used_reg_codes) {
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

uint8_t get_random_reg_codes(std::mt19937 &rng, bool unique_regs,
                             std::vector<uint8_t> used_reg_codes) {
    uint8_t reg_code1 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    used_reg_codes.push_back(reg_code1);
    uint8_t reg_code2 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    return (reg_code1 << 4) | reg_code2;
}

uint32_t get_random_imm32(std::mt19937 &rng, uint32_t min_val,
                          uint32_t max_val) {
    std::uniform_int_distribution<uint32_t> val_dist(min_val, max_val);
    return val_dist(rng);
}

bool get_random_bool(std::mt19937 &rng) {
    std::uniform_int_distribution<uint32_t> val_dist(0, 1);
    return val_dist(rng) == 0;
}
