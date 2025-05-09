/**
 * \file cpu_exec.c
 * CPU instruction execution implementation
 */

#include "cpu_exec.h"
#include "cpu_stack.h"
#include "debugm.h"

static vm_err_t prv_cpu_execute_data_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_alu_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_flow_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_stack_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_other_instr(cpu_ctx_t *cpu);

static void prv_cpu_set_flags(cpu_ctx_t *cpu, bool zero, bool sign, bool carry,
                              bool overflow);

vm_err_t cpu_execute_instr(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    vm_err_t err = VM_ERR_NONE;

    uint8_t opcode_kind = cpu->instr.opcode & CPU_OP_KIND_MASK;
    if (opcode_kind == CPU_OP_KIND_DATA) {
        err = prv_cpu_execute_data_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_ALU) {
        err = prv_cpu_execute_alu_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_FLOW) {
        err = prv_cpu_execute_flow_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_STACK) {
        err = prv_cpu_execute_stack_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_OTHER) {
        err = prv_cpu_execute_other_instr(cpu);
    } else {
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    return err;
}

static vm_err_t prv_cpu_execute_data_instr(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    vm_err_t err = VM_ERR_NONE;

    switch (cpu->instr.opcode) {
    case CPU_OP_MOV_VR: {
        uint32_t *p_reg = cpu->instr.operands[0].p_reg;
        *p_reg = cpu->instr.operands[1].u32;
        break;
    }
    case CPU_OP_MOV_RR: {
        uint32_t *p_reg_src = cpu->instr.operands[0].p_regs[0];
        uint32_t *p_reg_dst = cpu->instr.operands[0].p_regs[1];
        *p_reg_dst = *p_reg_src;
        break;
    }
    case CPU_OP_STR_RV0:
    case CPU_OP_LDR_RV0: {
        uint32_t *p_reg = cpu->instr.operands[0].p_reg;
        uint32_t mem_addr = cpu->instr.operands[1].u32;
        if (cpu->instr.opcode == CPU_OP_STR_RV0) {
            err = cpu->mem->write_u32(cpu->mem, mem_addr, *p_reg);
        } else {
            err = cpu->mem->read_u32(cpu->mem, mem_addr, p_reg);
        }
        break;
    }
    case CPU_OP_STR_RI0:
    case CPU_OP_LDR_RI0: {
        uint32_t *p_reg_src = cpu->instr.operands[0].p_regs[0];
        uint32_t *p_reg_dst = cpu->instr.operands[0].p_regs[1];
        if (cpu->instr.opcode == CPU_OP_STR_RI0) {
            err = cpu->mem->write_u32(cpu->mem, *p_reg_dst, *p_reg_src);
        } else {
            err = cpu->mem->read_u32(cpu->mem, *p_reg_src, p_reg_dst);
        }
        break;
    }
    case CPU_OP_STR_RI8:
    case CPU_OP_LDR_RI8: {
        uint32_t *p_reg_src = cpu->instr.operands[0].p_regs[0];
        uint32_t *p_reg_dst = cpu->instr.operands[0].p_regs[1];
        int8_t mem_offset = (int8_t)cpu->instr.operands[1].u8;
        if (cpu->instr.opcode == CPU_OP_STR_RI8) {
            err = cpu->mem->write_u32(cpu->mem, *p_reg_dst + mem_offset,
                                      *p_reg_src);
        } else {
            err = cpu->mem->read_u32(cpu->mem, *p_reg_src + mem_offset,
                                     p_reg_dst);
        }
        break;
    }
    case CPU_OP_STR_RI32:
    case CPU_OP_LDR_RI32: {
        uint32_t *p_reg_src = cpu->instr.operands[0].p_regs[0];
        uint32_t *p_reg_dst = cpu->instr.operands[0].p_regs[1];
        int32_t mem_offset = (int32_t)cpu->instr.operands[1].u32;
        if (cpu->instr.opcode == CPU_OP_STR_RI32) {
            err = cpu->mem->write_u32(cpu->mem, *p_reg_dst + mem_offset,
                                      *p_reg_src);
        } else {
            err = cpu->mem->read_u32(cpu->mem, *p_reg_src + mem_offset,
                                     p_reg_dst);
        }
        break;
    }
    case CPU_OP_STR_RIR:
    case CPU_OP_LDR_RIR: {
        uint32_t *p_reg_src = cpu->instr.operands[0].p_regs[0];
        uint32_t *p_reg_dst = cpu->instr.operands[0].p_regs[1];
        int32_t *p_reg_off = (int32_t *)cpu->instr.operands[1].p_reg;
        if (cpu->instr.opcode == CPU_OP_STR_RIR) {
            err = cpu->mem->write_u32(cpu->mem, *p_reg_dst + *p_reg_off,
                                      *p_reg_src);
        } else {
            err = cpu->mem->read_u32(cpu->mem, *p_reg_src + *p_reg_off,
                                     p_reg_dst);
        }
        break;
    }

    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    return err;
}

static vm_err_t prv_cpu_execute_alu_instr(cpu_ctx_t *cpu) {
    vm_err_t err = VM_ERR_NONE;

    uint32_t *p_reg_dst;
    uint32_t src_val;
    if ((cpu->instr.opcode & 1) == 0) {
        // Even opcodes require two register operands.
        p_reg_dst = cpu->instr.operands[0].p_regs[0];
        src_val = *cpu->instr.operands[0].p_regs[1];
    } else if (cpu->instr.opcode == CPU_OP_NOT_R) {
        // Only one register operand.
        p_reg_dst = cpu->instr.operands[0].p_reg;
    } else {
        // Odd opcodes require a register operand and an imm32 value, except for
        // some opcodes.
        p_reg_dst = cpu->instr.operands[0].p_reg;

        if (cpu->instr.opcode == CPU_OP_SHL_RV ||
            cpu->instr.opcode == CPU_OP_SHR_RV ||
            cpu->instr.opcode == CPU_OP_ROL_RV ||
            cpu->instr.opcode == CPU_OP_ROR_RV) {
            // Second operand is an imm5.
            src_val = cpu->instr.operands[1].imm5;
        } else {
            // Second operand is an imm32.
            src_val = cpu->instr.operands[1].u32;
        }
    }

    bool flag_zero = false;
    bool flag_sign = false;
    bool flag_carry = false;
    bool flag_ovf = false;

    switch (cpu->instr.opcode) {
    case CPU_OP_ADD_RR:
    case CPU_OP_ADD_RV: {
        uint64_t res = (uint64_t)*p_reg_dst + src_val;
        bool sign_op1 = (*p_reg_dst & (1 << 31)) != 0;
        bool sign_op2 = (src_val & (1 << 31)) != 0;
        bool sign_res = (res & (1L << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = (res & ~0xFFFFFFFFL) != 0;
        flag_ovf = (sign_op1 == sign_op2) && (sign_res != sign_op1);
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_SUB_RR:
    case CPU_OP_SUB_RV: {
        uint32_t res = *p_reg_dst - src_val;
        bool sign_op1 = (*p_reg_dst & (1 << 31)) != 0;
        bool sign_op2 = (src_val & (1 << 31)) != 0;
        bool sign_res = (res & (1L << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = *p_reg_dst >= src_val;
        flag_ovf = (sign_op1 != sign_op2) && (sign_res != sign_op1);
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_MUL_RR:
    case CPU_OP_MUL_RV: {
        uint64_t res = (uint64_t)*p_reg_dst * src_val;
        flag_zero = res == 0;
        flag_sign = (res & (1L << 31)) != 0;
        flag_carry = (res & ~0xFFFFFFFFL) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_DIV_RR:
    case CPU_OP_DIV_RV:
    case CPU_OP_IDIV_RR:
    case CPU_OP_IDIV_RV: {
        if (src_val == 0) {
            err = VM_ERR_DIV_BY_ZERO;
            return err;
        }
        uint32_t res;
        if (cpu->instr.opcode == CPU_OP_DIV_RR ||
            cpu->instr.opcode == CPU_OP_DIV_RV) {
            res = *p_reg_dst / src_val;
        } else {
            res = (int32_t)*p_reg_dst / (int32_t)src_val;
        }
        flag_zero = res == 0;
        flag_sign = (res & (1L << 31)) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_AND_RR:
    case CPU_OP_AND_RV: {
        uint32_t res = *p_reg_dst & src_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_OR_RR:
    case CPU_OP_OR_RV: {
        uint32_t res = *p_reg_dst | src_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_XOR_RR:
    case CPU_OP_XOR_RV: {
        uint32_t res = *p_reg_dst ^ src_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_NOT_R: {
        uint32_t res = ~*p_reg_dst;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        *p_reg_dst = (uint32_t)res;
        break;
    }

    case CPU_OP_SHL_RR:
    case CPU_OP_SHL_RV: {
        uint32_t numbits = src_val & 31;
        uint32_t res = *p_reg_dst << numbits;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        if (numbits > 0) {
            flag_carry = (*p_reg_dst & (1 << (32 - numbits))) != 0;
        }
        *p_reg_dst = res;
        break;
    }

    case CPU_OP_SHR_RR:
    case CPU_OP_SHR_RV: {
        uint32_t numbits = src_val & 31;
        uint32_t res = *p_reg_dst >> numbits;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        if (numbits > 0) {
            flag_carry = (*p_reg_dst & (1 << (-1 + numbits))) != 0;
        }
        *p_reg_dst = res;
        break;
    }

    case CPU_OP_CMP_RR: {
        // Same as SUB_RR, except that the dest reg is not written to.
        uint32_t res = *p_reg_dst - src_val;
        bool sign_op1 = (*p_reg_dst & (1 << 31)) != 0;
        bool sign_op2 = (src_val & (1 << 31)) != 0;
        bool sign_res = (res & (1L << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = *p_reg_dst >= src_val;
        flag_ovf = (sign_op1 != sign_op2) && (sign_res != sign_op1);
        break;
    }

    case CPU_OP_TST_RR:
    case CPU_OP_TST_RV: {
        // Same as AND_*, except that the dest reg is not written to.
        uint32_t res = *p_reg_dst & src_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        break;
    }

    case CPU_OP_ROL_RR:
    case CPU_OP_ROL_RV:
    case CPU_OP_ROR_RR:
    case CPU_OP_ROR_RV:
    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    if (err == VM_ERR_NONE) {
        prv_cpu_set_flags(cpu, flag_zero, flag_sign, flag_carry, flag_ovf);
    }
    return err;
}

static vm_err_t prv_cpu_execute_flow_instr(cpu_ctx_t *cpu) {
    vm_err_t err = VM_ERR_NONE;

    bool do_jump;
    bool flag_zero = (cpu->flags & CPU_FLAG_ZERO) != 0;
    bool flag_sign = (cpu->flags & CPU_FLAG_SIGN) != 0;
    bool flag_overflow = (cpu->flags & CPU_FLAG_OVERFLOW) != 0;
    bool flag_S_xor_V = flag_sign ^ flag_overflow;
    switch (cpu->instr.opcode) {
    case CPU_OP_JEQR_V8:
    case CPU_OP_JEQA_V32:
    case CPU_OP_JEQA_R:
        do_jump = flag_zero;
        break;

    case CPU_OP_JNER_V8:
    case CPU_OP_JNEA_V32:
    case CPU_OP_JNEA_R:
        do_jump = !flag_zero;
        break;

    case CPU_OP_JGTR_V8:
    case CPU_OP_JGTA_V32:
    case CPU_OP_JGTA_R:
        do_jump = !flag_zero && !flag_S_xor_V;
        break;

    case CPU_OP_JGER_V8:
    case CPU_OP_JGEA_V32:
    case CPU_OP_JGEA_R:
        do_jump = !flag_S_xor_V;
        break;

    case CPU_OP_JLTR_V8:
    case CPU_OP_JLTA_V32:
    case CPU_OP_JLTA_R:
        do_jump = flag_S_xor_V;
        break;

    case CPU_OP_JLER_V8:
    case CPU_OP_JLEA_V32:
    case CPU_OP_JLEA_R:
        do_jump = flag_zero || flag_S_xor_V;
        break;

    default:
        do_jump = true;
    }

    uint32_t jump_pc;
    switch (cpu->instr.opcode) {
    case CPU_OP_JMPR_V8:
    case CPU_OP_JEQR_V8:
    case CPU_OP_JNER_V8:
    case CPU_OP_JGTR_V8:
    case CPU_OP_JGER_V8:
    case CPU_OP_JLTR_V8:
    case CPU_OP_JLER_V8:
        jump_pc = cpu->instr.start_addr + (int8_t)cpu->instr.operands[0].u8;
        break;
    case CPU_OP_JMPA_V32:
    case CPU_OP_JEQA_V32:
    case CPU_OP_JNEA_V32:
    case CPU_OP_JGTA_V32:
    case CPU_OP_JGEA_V32:
    case CPU_OP_JLTA_V32:
    case CPU_OP_JLEA_V32:
        jump_pc = cpu->instr.operands[0].u32;
        break;
    case CPU_OP_JMPA_R:
    case CPU_OP_JEQA_R:
    case CPU_OP_JNEA_R:
    case CPU_OP_JGTA_R:
    case CPU_OP_JGEA_R:
    case CPU_OP_JLTA_R:
    case CPU_OP_JLEA_R:
        jump_pc = *cpu->instr.operands[0].p_reg;
        break;

    case CPU_OP_CALLA_V32:
    case CPU_OP_CALLA_R: {
        if (cpu->instr.opcode == CPU_OP_CALLA_V32) {
            jump_pc = cpu->instr.operands[0].u32;
        } else {
            jump_pc = *cpu->instr.operands[0].p_reg;
        }
        uint32_t next_instr_at = cpu->reg_pc;
        cpu_stack_push_u32(cpu, next_instr_at);
        break;
    }

    case CPU_OP_RET:
        cpu_stack_pop_u32(cpu, &jump_pc);
        break;

    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    if (err == VM_ERR_NONE && do_jump) { cpu->reg_pc = jump_pc; }
    return err;
}

static vm_err_t prv_cpu_execute_stack_instr(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    vm_err_t err = VM_ERR_NONE;

    switch (cpu->instr.opcode) {
    case CPU_OP_PUSH_V32:
        err = cpu_stack_push_u32(cpu, cpu->instr.operands[0].u32);
        break;

    case CPU_OP_PUSH_R:
        err = cpu_stack_push_u32(cpu, *cpu->instr.operands[0].p_reg);
        break;

    case CPU_OP_POP_R:
        err = cpu_stack_pop_u32(cpu, cpu->instr.operands[0].p_reg);
        break;

    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    return err;
}

static vm_err_t prv_cpu_execute_other_instr(cpu_ctx_t *cpu) {
    vm_err_t err = VM_ERR_NONE;
    switch (cpu->instr.opcode) {
    case CPU_OP_INT_V8:
        err = cpu_raise_irq(cpu, cpu->instr.operands[0].u8);
        break;

    case CPU_OP_HALT:
        cpu->state = CPU_HALTED;
        break;

    case CPU_OP_IRET:
        err = cpu_stack_pop_u32(cpu, &cpu->reg_pc);
        break;

    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }
    return err;
}

static void prv_cpu_set_flags(cpu_ctx_t *cpu, bool zero, bool sign, bool carry,
                              bool overflow) {
    D_ASSERT(cpu != NULL);
    if (zero) {
        cpu->flags |= CPU_FLAG_ZERO;
    } else {
        cpu->flags &= ~CPU_FLAG_ZERO;
    }
    if (sign) {
        cpu->flags |= CPU_FLAG_SIGN;
    } else {
        cpu->flags &= ~CPU_FLAG_SIGN;
    }
    if (carry) {
        cpu->flags |= CPU_FLAG_CARRY;
    } else {
        cpu->flags &= ~CPU_FLAG_CARRY;
    }
    if (overflow) {
        cpu->flags |= CPU_FLAG_OVERFLOW;
    } else {
        cpu->flags &= ~CPU_FLAG_OVERFLOW;
    }
}
