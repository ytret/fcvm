#pragma once

#include "vm.h"

#define CPU_OP_KIND_MASK 0xE0

/**
 * @{
 * \name Data movement opcodes (0b001x_xxxx)
 */
#define CPU_OP_KIND_DATA 0x20

#define CPU_OP_MOV_RR   0x20
#define CPU_OP_MOV_VR   0x21
#define CPU_OP_STR_RI0  0x22
#define CPU_OP_STR_RV0  0x23
#define CPU_OP_STR_RI8  0x24
#define CPU_OP_STR_RI32 0x25
#define CPU_OP_STR_RIR  0x26
#define CPU_OP_LDR_RV0  0x27
#define CPU_OP_LDR_RI0  0x28
#define CPU_OP_LDR_RI8  0x29
#define CPU_OP_LDR_RI32 0x2A
#define CPU_OP_LDR_RIR  0x2B
/// @}

/**
 * @{
 * \name Arithmetic and logic opcodes (0b010x_xxxx)
 * - Even opcodes require two register operands.
 * - Odd opcodes (except #CPU_NOT_R, #CPU_ROL_RV and #CPU_ROT_RV) require a
 *   register operand and an imm32 value.
 */
#define CPU_OP_KIND_ALU 0x40

#define CPU_OP_ADD_RR  0x42
#define CPU_OP_SUB_RR  0x44
#define CPU_OP_MUL_RR  0x46
#define CPU_OP_DIV_RR  0x48
#define CPU_OP_IDIV_RR 0x4A
#define CPU_OP_AND_RR  0x4C
#define CPU_OP_OR_RR   0x4E
#define CPU_OP_XOR_RR  0x50
#define CPU_OP_SHL_RR  0x52
#define CPU_OP_SHR_RR  0x54
#define CPU_OP_ROL_RR  0x56
#define CPU_OP_ROR_RR  0x58
#define CPU_OP_CMP_RR  0x5A
#define CPU_OP_TST_RR  0x5C

#define CPU_OP_ADD_RV  0x41
#define CPU_OP_SUB_RV  0x43
#define CPU_OP_MUL_RV  0x45
#define CPU_OP_DIV_RV  0x47
#define CPU_OP_IDIV_RV 0x49
#define CPU_OP_AND_RV  0x4B
#define CPU_OP_OR_RV   0x4D
#define CPU_OP_XOR_RV  0x4F
#define CPU_OP_SHL_RV  0x51
#define CPU_OP_SHR_RV  0x53
#define CPU_OP_TST_RV  0x55

#define CPU_OP_NOT_R  0x57
#define CPU_OP_ROL_RV 0x59
#define CPU_OP_ROR_RV 0x5B
/// @}

/**
 * \name Control flow opcodes (0b011x_xxxx)
 * Lower 2 bits indicate the operand type:
 * - `0b00` - imm8,
 * - `0b01` - imm32,
 * - `0b10` - register.
 * Except for #CPU_OP_RET, which does not have any operands.
 */
#define CPU_OP_KIND_FLOW 0x60

#define CPU_OP_JMPR_V8  (0x60 + 0)
#define CPU_OP_JMPA_V32 (0x60 + 1)
#define CPU_OP_JMPA_R   (0x60 + 2)
#define CPU_OP_JEQR_V8  (0x64 + 0)
#define CPU_OP_JEQA_V32 (0x64 + 1)
#define CPU_OP_JEQA_R   (0x64 + 2)
#define CPU_OP_JNER_V8  (0x68 + 0)
#define CPU_OP_JNEA_V32 (0x68 + 1)
#define CPU_OP_JNEA_R   (0x68 + 2)
#define CPU_OP_JGTR_V8  (0x6C + 0)
#define CPU_OP_JGTA_V32 (0x6C + 1)
#define CPU_OP_JGTA_R   (0x6C + 2)
#define CPU_OP_JGER_V8  (0x70 + 0)
#define CPU_OP_JGEA_V32 (0x70 + 1)
#define CPU_OP_JGEA_R   (0x70 + 2)
#define CPU_OP_JLTR_V8  (0x74 + 0)
#define CPU_OP_JLTA_V32 (0x74 + 1)
#define CPU_OP_JLTA_R   (0x74 + 2)
#define CPU_OP_JLER_V8  (0x78 + 0)
#define CPU_OP_JLEA_V32 (0x78 + 1)
#define CPU_OP_JLEA_R   (0x78 + 2)

#define CPU_OP_CALLA_V32 (0x7C + 1)
#define CPU_OP_CALLA_R   (0x7C + 2)

#define CPU_OP_RET 0x7F
/// @}

/**
 * \name Stack instructions
 */
#define CPU_OP_KIND_STACK 0x80

#define CPU_OP_PUSH_V32 0x80
#define CPU_OP_PUSH_R   0x81
#define CPU_OP_POP_R    0x82
/// @}

static_assert(VM_NUM_GP_REGS == 8, "please update register codec");
#define CPU_CODE_R0 0x00
#define CPU_CODE_R1 0x01
#define CPU_CODE_R2 0x02
#define CPU_CODE_R3 0x03
#define CPU_CODE_R4 0x04
#define CPU_CODE_R5 0x05
#define CPU_CODE_R6 0x06
#define CPU_CODE_R7 0x07
#define CPU_CODE_SP 0x20

#define CPU_FLAG_ZERO     (1 << 0)
#define CPU_FLAG_SIGN     (1 << 1)
#define CPU_FLAG_CARRY    (1 << 2)
#define CPU_FLAG_OVERFLOW (1 << 3)

void cpu_init(vm_state_t *vm);
void cpu_deinit(vm_state_t *vm);

void cpu_reset(vm_state_t *vm);
void cpu_step(vm_state_t *vm);
