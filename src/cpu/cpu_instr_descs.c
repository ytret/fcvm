/**
 * @file cpu_instr_descs.c
 * CPU instruction descriptor definitions.
 */

#include <fcvm/cpu_instr_descs.h>

static const cpu_instr_desc_t cpu_instr_descs[256] = {
    // clang-format off
    [CPU_OP_MOV_VR]   = {"MOV", CPU_OP_MOV_VR,   2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_MOV_RR]   = {"MOV", CPU_OP_MOV_RR,   2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_STR_RV0]  = {"STR", CPU_OP_STR_RV0,  2, {CPU_OPD_IMM32, CPU_OPD_REG}},
    [CPU_OP_STR_RI0]  = {"STR", CPU_OP_STR_RI0,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_STR_RI8]  = {"STR", CPU_OP_STR_RI8,  3, {CPU_OPD_REG, CPU_OPD_IMM8, CPU_OPD_REG}},
    [CPU_OP_STR_RI32] = {"STR", CPU_OP_STR_RI32, 3, {CPU_OPD_REG, CPU_OPD_IMM32, CPU_OPD_REG}},
    [CPU_OP_STR_RIR]  = {"STR", CPU_OP_STR_RIR,  3, {CPU_OPD_REG, CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_LDR_RV0]  = {"LDR", CPU_OP_LDR_RV0,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_LDR_RI0]  = {"LDR", CPU_OP_LDR_RI0,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_LDR_RI8]  = {"LDR", CPU_OP_LDR_RI8,  3, {CPU_OPD_REG, CPU_OPD_REG, CPU_OPD_IMM8}},
    [CPU_OP_LDR_RI32] = {"LDR", CPU_OP_LDR_RI32, 3, {CPU_OPD_REG, CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_LDR_RIR]  = {"LDR", CPU_OP_LDR_RIR,  3, {CPU_OPD_REG, CPU_OPD_REG, CPU_OPD_REG}},

    [CPU_OP_ADD_RR]  = {"ADD",  CPU_OP_ADD_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_ADD_RV]  = {"ADD",  CPU_OP_ADD_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_SUB_RR]  = {"SUB",  CPU_OP_SUB_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_SUB_RV]  = {"SUB",  CPU_OP_SUB_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_MUL_RR]  = {"MUL",  CPU_OP_MUL_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_MUL_RV]  = {"MUL",  CPU_OP_MUL_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_DIV_RR]  = {"DIV",  CPU_OP_DIV_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_DIV_RV]  = {"DIV",  CPU_OP_DIV_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_IDIV_RR] = {"IDIV", CPU_OP_IDIV_RR, 2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_IDIV_RV] = {"IDIV", CPU_OP_IDIV_RV, 2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_AND_RR]  = {"AND",  CPU_OP_AND_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_AND_RV]  = {"AND",  CPU_OP_AND_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_OR_RR]   = {"OR",   CPU_OP_OR_RR,   2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_OR_RV]   = {"OR",   CPU_OP_OR_RV,   2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_XOR_RR]  = {"XOR",  CPU_OP_XOR_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_XOR_RV]  = {"XOR",  CPU_OP_XOR_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},
    [CPU_OP_NOT_R]   = {"NOT",  CPU_OP_NOT_R,   1, {CPU_OPD_REG}},
    [CPU_OP_SHL_RR]  = {"SHL",  CPU_OP_SHL_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_SHL_RV]  = {"SHL",  CPU_OP_SHL_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM5}},
    [CPU_OP_SHR_RR]  = {"SHR",  CPU_OP_SHR_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_SHR_RV]  = {"SHR",  CPU_OP_SHR_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM5}},
    [CPU_OP_ROR_RR]  = {"ROR",  CPU_OP_ROR_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_ROR_RV]  = {"ROR",  CPU_OP_ROR_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM5}},
    [CPU_OP_ROL_RR]  = {"ROL",  CPU_OP_ROL_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_ROL_RV]  = {"ROL",  CPU_OP_ROL_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM5}},
    [CPU_OP_CMP_RR]  = {"CMP",  CPU_OP_CMP_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_TST_RR]  = {"TST",  CPU_OP_TST_RR,  2, {CPU_OPD_REG, CPU_OPD_REG}},
    [CPU_OP_TST_RV]  = {"TST",  CPU_OP_TST_RV,  2, {CPU_OPD_REG, CPU_OPD_IMM32}},

    [CPU_OP_JMPR_V8]   = {"JMPR",  CPU_OP_JMPR_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JMPA_V32]  = {"JMPA",  CPU_OP_JMPA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JMPA_R]    = {"JMPA",  CPU_OP_JMPA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JEQR_V8]   = {"JEQR",  CPU_OP_JEQR_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JEQA_V32]  = {"JEQA",  CPU_OP_JEQA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JEQA_R]    = {"JEQA",  CPU_OP_JEQA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JNER_V8]   = {"JNER",  CPU_OP_JNER_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JNEA_V32]  = {"JNEA",  CPU_OP_JNEA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JNEA_R]    = {"JNEA",  CPU_OP_JNEA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JGTR_V8]   = {"JGTR",  CPU_OP_JGTR_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JGTA_V32]  = {"JGTA",  CPU_OP_JGTA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JGTA_R]    = {"JGTA",  CPU_OP_JGTA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JGER_V8]   = {"JGER",  CPU_OP_JGER_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JGEA_V32]  = {"JGEA",  CPU_OP_JGEA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JGEA_R]    = {"JGEA",  CPU_OP_JGEA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JLTR_V8]   = {"JLTR",  CPU_OP_JLTR_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JLTA_V32]  = {"JLTA",  CPU_OP_JLTA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JLTA_R]    = {"JLTA",  CPU_OP_JLTA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_JLER_V8]   = {"JLER",  CPU_OP_JLER_V8,   1, {CPU_OPD_IMM8}},
    [CPU_OP_JLEA_V32]  = {"JLEA",  CPU_OP_JLEA_V32,  1, {CPU_OPD_IMM32}},
    [CPU_OP_JLEA_R]    = {"JLEA",  CPU_OP_JLEA_R,    1, {CPU_OPD_REG}},
    [CPU_OP_CALLA_V32] = {"CALLA", CPU_OP_CALLA_V32, 1, {CPU_OPD_IMM32}},
    [CPU_OP_CALLA_R]   = {"CALLA", CPU_OP_CALLA_R,   1, {CPU_OPD_REG}},
    [CPU_OP_RET]       = {"RET",   CPU_OP_RET,       0, {}},

    [CPU_OP_PUSH_V32] = {"PUSH", CPU_OP_PUSH_V32, 1, {CPU_OPD_IMM32}},
    [CPU_OP_PUSH_R]   = {"PUSH", CPU_OP_PUSH_R,   1, {CPU_OPD_REG}},
    [CPU_OP_POP_R]    = {"POP",  CPU_OP_POP_R,    1, {CPU_OPD_REG}},

    [CPU_OP_NOP]    = {"NOP",  CPU_OP_NOP,    0, {}},
    [CPU_OP_HALT]   = {"HALT", CPU_OP_HALT,   0, {}},
    [CPU_OP_INT_V8] = {"INT",  CPU_OP_INT_V8, 1, {CPU_OPD_IMM8}},
    [CPU_OP_IRET]   = {"IRET", CPU_OP_IRET,   0, {}},
    // clang-format on
};

const cpu_instr_desc_t *cpu_lookup_instr_desc(uint8_t opcode) {
    const cpu_instr_desc_t *desc = &cpu_instr_descs[opcode];
    // Treat opcode 0x00 in the LUT as a missing descriptor.
    if (desc->opcode == 0) {
        return NULL;
    } else {
        return desc;
    }
}
