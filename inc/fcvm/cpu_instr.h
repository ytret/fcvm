/**
 * @file cpu_instr.h
 * CPU instruction type definitions.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <fcvm/cpu_instr_descs.h>
#include <fcvm/vm_types.h>

/// Register access size.
typedef enum {
    CPU_REG_SIZE_8,  //!< Access the lower 8 bits of the register.
    CPU_REG_SIZE_32, //!< Access the whole 32 bits of the register.
} cpu_reg_size_t;

/// Decoded register reference.
typedef struct {
    /// Original reference byte, as it appeared in the bytecode.
    uint8_t encoded_ref;
    /// Register access size.
    cpu_reg_size_t access_size;
    /// Register code.
    uint8_t reg_code;
    union {
        /// Pointer to the register value in the CPU context. See #cpu_ctx_t.
        uint32_t *p_reg;
        /// Pointer to the lower 8 bits of the register in the CPU context.
        uint8_t *p_reg_u8;
    };
} cpu_reg_ref_t;

typedef union {
    cpu_reg_ref_t reg_ref;
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
