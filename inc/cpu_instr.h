#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cpu_instr_descs.h"
#include "vm_types.h"

typedef union {
    struct {
        uint8_t reg_code;
        uint32_t *p_reg;
    };
    struct {
        uint8_t reg_codes;
        uint32_t *p_regs[2];
    };
    uint8_t imm5;
    uint8_t u8;
    uint32_t u32;
} cpu_opd_val_t;

/// Instruction execution context.
typedef struct {
    vm_addr_t start_addr;                     //!< Address of the opcode.
    uint8_t opcode;                           //!< Fetched opcode value.
    cpu_opd_val_t operands[CPU_MAX_OPERANDS]; //!< Decoded operand values.

    size_t next_operand;          //!< Next operand to fetch and decode.
    const cpu_instr_desc_t *desc; //!< Descriptor of the instruction.
} cpu_instr_t;
