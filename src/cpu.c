#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "debugm.h"

static vm_err_t prv_cpu_fetch_decode_operand(cpu_ctx_t *cpu,
                                             cpu_operand_type_t opd_type,
                                             void *v_out);
static vm_err_t prv_cpu_execute_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_data_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_alu_instr(cpu_ctx_t *cpu);
static vm_err_t prv_cpu_execute_flow_instr(cpu_ctx_t *cpu);

static bool prv_cpu_check_err(cpu_ctx_t *cpu, vm_err_t err);
static void prv_cpu_raise_exception(cpu_ctx_t *cpu, vm_err_t err);
static void prv_cpu_set_flags(cpu_ctx_t *cpu, bool zero, bool sign, bool carry,
                              bool overflow);

cpu_ctx_t *cpu_new(mem_if_t *mem) {
    D_ASSERT(mem);

    cpu_ctx_t *cpu = malloc(sizeof(*cpu));
    D_ASSERT(cpu);
    memset(cpu, 0, sizeof(*cpu));

    cpu->state = CPU_FETCH_DECODE_OPCODE;
    cpu->mem = mem;

    return cpu;
}

void cpu_free(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    free(cpu);
}

void cpu_step(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);

    switch (cpu->state) {
    case CPU_FETCH_DECODE_OPCODE: {
        vm_err_t err =
            cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &cpu->instr.opcode);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }
        cpu->instr.desc = cpu_lookup_instr_desc(cpu->instr.opcode);
        if (cpu->instr.desc) {
            cpu->instr.start_addr = cpu->reg_pc;
            cpu->reg_pc += 1;
            if (cpu->instr.desc->num_operands == 0) {
                cpu->state = CPU_EXECUTE;
            } else {
                cpu->instr.next_operand = 0;
                cpu->state = CPU_FETCH_DECODE_OPERANDS;
            }
        } else {
            D_PRINTF("bad opcode: 0x%02X", cpu->instr.opcode);
            vm_err_t err = {.type = VM_ERR_BAD_OPCODE};
            prv_cpu_raise_exception(cpu, err);
        }
        break;
    }

    case CPU_FETCH_DECODE_OPERANDS: {
        D_ASSERT(cpu->instr.desc);
        size_t opd_idx = cpu->instr.next_operand;
        D_ASSERT(opd_idx < CPU_MAX_OPERANDS);
        D_ASSERT(opd_idx < cpu->instr.desc->num_operands);
        cpu_operand_type_t opd_type = cpu->instr.desc->operands[opd_idx];

        vm_err_t err = prv_cpu_fetch_decode_operand(
            cpu, opd_type, &cpu->instr.operands[opd_idx]);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }

        cpu->instr.next_operand++;
        if (cpu->instr.next_operand == cpu->instr.desc->num_operands) {
            cpu->state = CPU_EXECUTE;
        }
        break;
    }

    case CPU_EXECUTE: {
        vm_err_t err = prv_cpu_execute_instr(cpu);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }
        cpu->state = CPU_EXECUTED_OK;
        break;
    }

    case CPU_EXECUTED_OK:
        D_ASSERTMF(false,
                   "cpu_step() must not be called when cpu->state is "
                   "CPU_EXECUTED_OK (%u)",
                   cpu->state);

    case CPU_HANDLE_INT:
        D_TODO();
        break;

    default:
        D_ASSERTMF(false, "cpu_step: unknown state: %d", cpu->state);
    }

CPU_STEP_END:
}

vm_err_t cpu_decode_reg(cpu_ctx_t *cpu, uint8_t reg_code,
                        uint32_t **out_reg_ptr) {
    D_ASSERT(cpu);
    D_ASSERT(out_reg_ptr);
    vm_err_t err = {.type = VM_ERR_NONE};

    uint32_t *code_addr_map[0xFF] = {
        [CPU_CODE_R0] = &cpu->gp_regs[0], [CPU_CODE_R1] = &cpu->gp_regs[1],
        [CPU_CODE_R2] = &cpu->gp_regs[2], [CPU_CODE_R3] = &cpu->gp_regs[3],
        [CPU_CODE_R4] = &cpu->gp_regs[4], [CPU_CODE_R5] = &cpu->gp_regs[5],
        [CPU_CODE_R6] = &cpu->gp_regs[6], [CPU_CODE_R7] = &cpu->gp_regs[7],
        [CPU_CODE_SP] = &cpu->reg_sp,
    };
    static_assert(CPU_NUM_GP_REG_CODES == 8, "update register decoding");

    uint32_t *p_reg = code_addr_map[reg_code];
    if (p_reg) {
        *out_reg_ptr = p_reg;
    } else {
        D_PRINTF("bad register code: 0x%02X", reg_code);
        err.type = VM_ERR_BAD_REG_CODE;
        *out_reg_ptr = NULL;
    }
    return err;
}

static vm_err_t prv_cpu_fetch_decode_operand(cpu_ctx_t *cpu,
                                             cpu_operand_type_t opd_type,
                                             void *v_out) {
    D_ASSERT(cpu);
    D_ASSERT(v_out);
    vm_err_t err = {.type = VM_ERR_NONE};
    uint32_t opd_size = 0;

    switch (opd_type) {
    case CPU_OPD_REG: {
        uint8_t reg_code;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &reg_code);
        if (err.type) { return err; }

        uint32_t *p_reg;
        err = cpu_decode_reg(cpu, reg_code, &p_reg);
        if (err.type) { return err; }

        uint32_t **out_reg = (uint32_t **)v_out;
        *out_reg = p_reg;

        opd_size = 1;
        break;
    }
    case CPU_OPD_REGS: {
        uint8_t reg_codes;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &reg_codes);
        if (err.type) { return err; }

        uint32_t *p_reg1;
        uint32_t *p_reg2;
        err = cpu_decode_reg(cpu, (reg_codes >> 4) & 0x0F, &p_reg1);
        if (err.type) { return err; }
        err = cpu_decode_reg(cpu, (reg_codes >> 0) & 0x0F, &p_reg2);
        if (err.type) { return err; }

        uint32_t **out_regs = (uint32_t **)v_out;
        out_regs[0] = p_reg1;
        out_regs[1] = p_reg2;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM5: {
        uint8_t imm5;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &imm5);
        if (err.type) { return err; }
        if ((imm5 & ~31) != 0) {
            err.type = VM_ERR_BAD_IMM5;
            return err;
        }

        uint8_t *out_imm5 = (uint8_t *)v_out;
        *out_imm5 = imm5;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM8: {
        uint8_t imm8;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &imm8);
        if (err.type) { return err; }

        uint32_t *out_imm8 = (uint32_t *)v_out;
        *out_imm8 = imm8;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM32: {
        uint32_t imm32;
        err = cpu->mem->read_u32(cpu->mem, cpu->reg_pc, &imm32);
        if (err.type) { return err; }

        uint32_t *out_imm32 = (uint32_t *)v_out;
        *out_imm32 = imm32;
        opd_size = 4;
        break;
    }
    default:
        D_ASSERTMF(false, "invalid opd_type %d", opd_type);
    }

    cpu->reg_pc += opd_size;
    return err;
}

static vm_err_t prv_cpu_execute_instr(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    vm_err_t err = {.type = VM_ERR_NONE};

    uint8_t opcode_kind = cpu->instr.opcode & CPU_OP_KIND_MASK;
    if (opcode_kind == CPU_OP_KIND_DATA) {
        err = prv_cpu_execute_data_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_ALU) {
        err = prv_cpu_execute_alu_instr(cpu);
    } else if (opcode_kind == CPU_OP_KIND_FLOW) {
        err = prv_cpu_execute_flow_instr(cpu);
    } else {
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }

    return err;
}

static vm_err_t prv_cpu_execute_data_instr(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    vm_err_t err = {.type = VM_ERR_NONE};

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
    vm_err_t err = {.type = VM_ERR_NONE};

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
            err.type = VM_ERR_DIV_BY_ZERO;
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

    if (err.type == VM_ERR_NONE) {
        prv_cpu_set_flags(cpu, flag_zero, flag_sign, flag_carry, flag_ovf);
    }
    return err;
}

static vm_err_t prv_cpu_execute_flow_instr(cpu_ctx_t *cpu) {
    vm_err_t err = {.type = VM_ERR_NONE};
    switch (cpu->instr.opcode) {
    case CPU_OP_JMPR_V8:
        cpu->reg_pc = cpu->instr.start_addr + (int8_t)cpu->instr.operands[0].u8;
        break;

    case CPU_OP_JMPA_V32:
        cpu->reg_pc = cpu->instr.operands[0].u32;
        break;

    default:
        D_ASSERTMF(false, "instruction is not implemented: 0x%02X",
                   cpu->instr.opcode);
    }
    return err;
}

static bool prv_cpu_check_err(cpu_ctx_t *cpu, vm_err_t err) {
    D_ASSERT(cpu);
    if (err.type != VM_ERR_NONE) {
        prv_cpu_raise_exception(cpu, err);
        return true;
    } else {
        return false;
    }
}

static void prv_cpu_raise_exception(cpu_ctx_t *cpu, vm_err_t err) {
    D_ASSERT(cpu);
    D_ASSERT(cpu->raise_irq);
    cpu->raise_irq(cpu, vm_err_to_irq_line(err));
    cpu->state = CPU_HANDLE_INT;
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
