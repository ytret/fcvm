#pragma once

#include <stdint.h>

#include "cpu_instr_descs.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CPU_NUM_GP_REGS 8
static_assert(CPU_NUM_GP_REGS == CPU_NUM_GP_REG_CODES,
              "update register codes in cpu_instr_descs.h");

#define CPU_FLAG_ZERO     (1 << 0)
#define CPU_FLAG_SIGN     (1 << 1)
#define CPU_FLAG_CARRY    (1 << 2)
#define CPU_FLAG_OVERFLOW (1 << 3)

#define CPU_IVT_ADDR        ((vm_addr_t)0x0000'0000)
#define CPU_IVT_ENTRY_SIZE  sizeof(vm_addr_t)
#define CPU_IVT_NUM_ENTRIES 256
#define CPU_IVT_ENTRY_ADDR(entry_idx)                                          \
    (CPU_IVT_ADDR + CPU_IVT_ENTRY_SIZE * (entry_idx))

typedef enum {
    CPU_RESET,
    CPU_FETCH_DECODE_OPCODE,
    CPU_FETCH_DECODE_OPERANDS,
    CPU_EXECUTE,
    CPU_EXECUTED_OK,

    CPU_INT_FETCH_ISR_ADDR,
    CPU_INT_PUSH_PC,
    CPU_INT_JUMP,
    CPU_TRIPLE_FAULT,
} cpu_state_t;

/// Instruction execution context.
typedef struct {
    vm_addr_t start_addr; //!< Address of the opcode.
    uint8_t opcode;       //!< Fetched opcode value.
    union {
        uint32_t *p_reg;
        uint32_t *p_regs[2];
        uint8_t imm5;
        uint8_t u8;
        uint32_t u32;
    } operands[CPU_MAX_OPERANDS]; //!< Decoded operand values.

    size_t next_operand;          //!< Next operand to fetch and decode.
    const cpu_instr_desc_t *desc; //!< Descriptor of the instruction.
} cpu_instr_t;

/// Exception numbers.
/// These must fit into `uint8_t`.
typedef enum {
    CPU_EXC_RESET,
    CPU_EXC_BAD_MEM,
    CPU_EXC_BAD_INSTR,
    CPU_EXC_DIV_BY_ZERO,
    CPU_EXC_STACK_OVERFLOW,

    CPU_NUM_EXCEPTIONS,
} cpu_exc_type_t;
static_assert(CPU_NUM_EXCEPTIONS <= 256,
              "cpu_exc_type does not fit into uint8_t");

typedef struct cpu_ctx {
    cpu_state_t state;
    cpu_instr_t instr;

    uint32_t gp_regs[CPU_NUM_GP_REGS];
    uint32_t reg_pc;
    uint32_t reg_sp;
    uint8_t flags;
    uint64_t cycles;

    mem_if_t *mem;

    size_t num_nested_exc;
    uint8_t curr_int_line;
    vm_addr_t curr_isr_addr;
    uint32_t pc_after_isr;
} cpu_ctx_t;

cpu_ctx_t *cpu_new(mem_if_t *mem);
void cpu_free(cpu_ctx_t *cpu);

void cpu_step(cpu_ctx_t *cpu);

vm_err_t cpu_decode_reg(cpu_ctx_t *cpu, uint8_t reg_code,
                        uint32_t **out_reg_ptr);
cpu_exc_type_t cpu_exc_type_of_err(cpu_ctx_t *cpu, vm_err_type_t err_type);

#ifdef __cplusplus
}
#endif
