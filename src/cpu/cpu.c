/**
 * @file cpu.c
 * CPU core implementation: instruction fetching, decoding and execution.
 */

#include <stdlib.h>
#include <string.h>

#include <fcvm/cpu.h>

#include "cpu_exec.h"
#include "cpu_stack.h"
#include "debugm.h"

static vm_err_t prv_cpu_fetch_decode_operand(cpu_ctx_t *cpu,
                                             cpu_operand_type_t opd_type,
                                             cpu_opd_val_t *out_val);
static void prv_cpu_print_instr(const cpu_instr_t *instr);

static bool prv_cpu_check_err(cpu_ctx_t *cpu, vm_err_t err);
static void prv_cpu_raise_exception(cpu_ctx_t *cpu, vm_err_t err);

cpu_ctx_t *cpu_new(mem_if_t *mem) {
    D_ASSERT(mem);

    cpu_ctx_t *cpu = malloc(sizeof(*cpu));
    D_ASSERT(cpu);
    memset(cpu, 0, sizeof(*cpu));

    cpu->state = CPU_RESET;
    cpu->mem = mem;
    cpu->intctl = intctl_new();
    cpu->num_nested_exc = 0;

    return cpu;
}

void cpu_free(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    intctl_free(cpu->intctl);
    free(cpu);
}

size_t cpu_snapshot_size(void) {
    static_assert(SN_CPU_CTX_VER == 1);
    return sizeof(cpu_ctx_t) + intctl_snapshot_size();
}

size_t cpu_snapshot(const cpu_ctx_t *cpu, void *v_buf, size_t max_size) {
    static_assert(SN_CPU_CTX_VER == 1);
    D_ASSERT(cpu);
    D_ASSERT(v_buf);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t size = 0;

    // Replace every pointer by NULL.
    cpu_ctx_t cpu_copy;
    memcpy(&cpu_copy, cpu, sizeof(cpu_copy));
    cpu_copy.mem = NULL;
    cpu_copy.intctl = NULL;

    // Write the CPU context.
    D_ASSERT(size + sizeof(cpu_copy) <= max_size);
    memcpy(&buf[size], &cpu_copy, sizeof(cpu_copy));
    size += sizeof(cpu_copy);

    // Write the intctl context.
    size += intctl_snapshot(cpu->intctl, &buf[size], max_size - size);

    return size;
}

cpu_ctx_t *cpu_restore(mem_if_t *mem, const void *v_buf, size_t max_size,
                       size_t *out_used_size) {
    static_assert(SN_CPU_CTX_VER == 1);
    D_ASSERT(mem);
    D_ASSERT(v_buf);
    D_ASSERT(out_used_size);
    uint8_t *buf = (uint8_t *)v_buf;
    size_t offset = 0;

    // Restore the CPU context.
    cpu_ctx_t rest_cpu;
    D_ASSERT(offset + sizeof(rest_cpu) <= max_size);
    memcpy(&rest_cpu, &buf[offset], sizeof(rest_cpu));
    offset += sizeof(rest_cpu);

    // Create a new CPU and set the fields manually.
    cpu_ctx_t *cpu = cpu_new(mem);
    cpu->state = rest_cpu.state;
    cpu->instr = rest_cpu.instr;
    memcpy(cpu->gp_regs, rest_cpu.gp_regs, sizeof(cpu->gp_regs));
    cpu->reg_pc = rest_cpu.reg_pc;
    cpu->reg_sp = rest_cpu.reg_sp;
    cpu->flags = rest_cpu.flags;
    cpu->cycles = rest_cpu.cycles;
    cpu->num_nested_exc = rest_cpu.num_nested_exc;
    cpu->curr_int_line = rest_cpu.curr_int_line;
    cpu->curr_isr_addr = rest_cpu.curr_isr_addr;
    cpu->pc_after_isr = rest_cpu.pc_after_isr;

    // Restore the register pointers in the instruction execution context.
    if (cpu->state == CPU_FETCH_DECODE_OPERANDS || cpu->state == CPU_EXECUTE) {
        cpu->instr.desc = cpu_lookup_instr_desc(cpu->instr.opcode);
        D_ASSERT(cpu->instr.desc);
        for (size_t opd = 0; opd < cpu->instr.next_operand; opd++) {
            if (cpu->instr.desc->operands[opd] == CPU_OPD_REG) {
                cpu_decode_reg(cpu, cpu->instr.operands[opd].reg_code,
                               &cpu->instr.operands[opd].p_reg);
                D_ASSERT(cpu->instr.operands[opd].p_reg);
            } else if (cpu->instr.desc->operands[opd] == CPU_OPD_REGS) {
                cpu_decode_reg(cpu,
                               (cpu->instr.operands[opd].reg_codes >> 4) & 0x0F,
                               &cpu->instr.operands[opd].p_regs[0]);
                cpu_decode_reg(cpu,
                               (cpu->instr.operands[opd].reg_codes >> 0) & 0x0F,
                               &cpu->instr.operands[opd].p_regs[1]);
                D_ASSERT(cpu->instr.operands[opd].p_regs[0]);
                D_ASSERT(cpu->instr.operands[opd].p_regs[1]);
            }
        }
    }

    // Delete the intctl allocated in cpu_new() and restore it from the buf.
    intctl_free(cpu->intctl);
    size_t intctl_size = 0;
    cpu->intctl = intctl_restore(&buf[offset], max_size - offset, &intctl_size);
    offset += intctl_size;

    *out_used_size = offset;
    return cpu;
}

void cpu_step(cpu_ctx_t *cpu) {
    D_ASSERT(cpu);
    D_ASSERT(cpu->intctl);

    if (cpu->state == CPU_FETCH_DECODE_OPCODE || cpu->state == CPU_HALTED) {
        if (intctl_has_pending_irqs(cpu->intctl)) {
            uint8_t pending_irq;
            if (intctl_get_pending_irq(cpu->intctl, &pending_irq)) {
                cpu->curr_int_line = CPU_IVT_FIRST_IRQ_ENTRY + pending_irq;
                cpu->pc_after_isr = cpu->reg_pc;
                cpu->state = CPU_INT_FETCH_ISR_ADDR;
            }
        }
    }

    switch (cpu->state) {
    case CPU_RESET: {
        cpu->curr_int_line = 0;
        cpu->num_nested_exc = 0;
        cpu->state = CPU_INT_FETCH_ISR_ADDR;
        break;
    }

    case CPU_FETCH_DECODE_OPCODE: {
        cpu->instr.start_addr = cpu->reg_pc;
        vm_err_t err =
            cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &cpu->instr.opcode);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }
        cpu->instr.desc = cpu_lookup_instr_desc(cpu->instr.opcode);
        if (cpu->instr.desc) {
            cpu->reg_pc += 1;
            if (cpu->instr.desc->num_operands == 0) {
                cpu->state = CPU_EXECUTE;
            } else {
                cpu->instr.next_operand = 0;
                cpu->state = CPU_FETCH_DECODE_OPERANDS;
            }
        } else {
            D_PRINTF("bad opcode 0x%02X at 0x%08X", cpu->instr.opcode,
                     cpu->reg_pc);
            vm_err_t err = VM_ERR_BAD_OPCODE;
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
        prv_cpu_print_instr(&cpu->instr);
        vm_err_t err = cpu_execute_instr(cpu);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }
        if (cpu->state == CPU_EXECUTE) {
            // If the state has not been changed by the instruction (e.g. HALT),
            // fetch and decode the next opcode.
            cpu->state = CPU_FETCH_DECODE_OPCODE;
        }
        break;
    }

    case CPU_HALTED:
        break;

    case CPU_INT_FETCH_ISR_ADDR: {
        uint8_t entry_idx = cpu->curr_int_line;
        vm_addr_t entry_addr = CPU_IVT_ENTRY_ADDR(entry_idx);

        vm_err_t err =
            cpu->mem->read_u32(cpu->mem, entry_addr, &cpu->curr_isr_addr);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }

        if (cpu->curr_int_line == 0) {
            cpu->state = CPU_INT_JUMP;
        } else {
            cpu->state = CPU_INT_PUSH_PC;
        }
        break;
    }

    case CPU_INT_PUSH_PC: {
        vm_err_t err = cpu_stack_push_u32(cpu, cpu->pc_after_isr);
        if (prv_cpu_check_err(cpu, err)) { goto CPU_STEP_END; }
        cpu->state = CPU_INT_JUMP;
        break;
    }

    case CPU_INT_JUMP: {
        cpu->reg_pc = cpu->curr_isr_addr;
        cpu->state = CPU_FETCH_DECODE_OPCODE;
        break;
    }

    case CPU_TRIPLE_FAULT:
        D_PRINT("cpu triple fault");
        cpu->state = CPU_RESET;
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
    vm_err_t err = VM_ERR_NONE;

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
        err = VM_ERR_BAD_REG_CODE;
        *out_reg_ptr = NULL;
    }
    return err;
}

cpu_exc_type_t cpu_exc_type_of_err(cpu_ctx_t *cpu, vm_err_t err) {
    D_ASSERT(cpu);
    switch (err) {
    case VM_ERR_BAD_MEM:
    case VM_ERR_MEM_BAD_OP:
        return CPU_EXC_BAD_MEM;

    case VM_ERR_BAD_OPCODE:
    case VM_ERR_BAD_REG_CODE:
    case VM_ERR_BAD_IMM5:
    case VM_ERR_INVALID_IRQ_NUM:
        return CPU_EXC_BAD_INSTR;

    case VM_ERR_DIV_BY_ZERO:
        return CPU_EXC_DIV_BY_ZERO;

    case VM_ERR_STACK_OVERFLOW:
        return CPU_EXC_STACK_OVERFLOW;

    case VM_ERR_NONE:
        D_ASSERTM(false,
                  "cpu_exc_num_of_err must not be called with VM_ERR_NONE");
    default:
        D_ASSERT(false);
    }
}

vm_err_t cpu_raise_irq(cpu_ctx_t *cpu, uint8_t irq_line) {
    D_ASSERT(cpu);
    D_ASSERT(cpu->intctl);
    return intctl_raise_irq_line(cpu->intctl, irq_line);
}

static vm_err_t prv_cpu_fetch_decode_operand(cpu_ctx_t *cpu,
                                             cpu_operand_type_t opd_type,
                                             cpu_opd_val_t *out_val) {
    D_ASSERT(cpu);
    D_ASSERT(out_val);
    vm_err_t err = VM_ERR_NONE;
    uint32_t opd_size = 0;

    switch (opd_type) {
    case CPU_OPD_REG: {
        uint8_t reg_code;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &reg_code);
        if (err) { return err; }

        uint32_t *p_reg;
        err = cpu_decode_reg(cpu, reg_code, &p_reg);
        if (err) { return err; }

        out_val->reg_code = reg_code;
        out_val->p_reg = p_reg;

        opd_size = 1;
        break;
    }
    case CPU_OPD_REGS: {
        uint8_t reg_codes;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &reg_codes);
        if (err) { return err; }

        uint32_t *p_reg1;
        uint32_t *p_reg2;
        err = cpu_decode_reg(cpu, (reg_codes >> 4) & 0x0F, &p_reg1);
        if (err) { return err; }
        err = cpu_decode_reg(cpu, (reg_codes >> 0) & 0x0F, &p_reg2);
        if (err) { return err; }

        out_val->reg_codes = reg_codes;
        out_val->p_regs[0] = p_reg1;
        out_val->p_regs[1] = p_reg2;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM5: {
        uint8_t imm5;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &imm5);
        if (err) { return err; }
        if ((imm5 & ~31) != 0) {
            err = VM_ERR_BAD_IMM5;
            return err;
        }

        out_val->imm5 = imm5;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM8: {
        uint8_t imm8;
        err = cpu->mem->read_u8(cpu->mem, cpu->reg_pc, &imm8);
        if (err) { return err; }

        out_val->u8 = imm8;
        opd_size = 1;
        break;
    }
    case CPU_OPD_IMM32: {
        uint32_t imm32;
        err = cpu->mem->read_u32(cpu->mem, cpu->reg_pc, &imm32);
        if (err) { return err; }

        out_val->u32 = imm32;
        opd_size = 4;
        break;
    }
    default:
        D_ASSERTMF(false, "invalid opd_type %d", opd_type);
    }

    cpu->reg_pc += opd_size;
    return err;
}

static void prv_cpu_print_instr(const cpu_instr_t *instr) {
    D_ASSERT(instr);
    D_ASSERT(instr->desc);

    fprintf(stderr, "%08X | %02X", instr->start_addr, instr->opcode);
    for (size_t opd = 0; opd < instr->desc->num_operands; opd++) {
        if (opd == 0) { fprintf(stderr, " ["); }
        switch (instr->desc->operands[opd]) {
        case CPU_OPD_REG:
            fprintf(stderr, "reg %02X", instr->operands[opd].reg_code);
            break;
        case CPU_OPD_REGS:
            fprintf(stderr, "regs %02X", instr->operands[opd].reg_codes);
            break;
        case CPU_OPD_IMM5:
            fprintf(stderr, "imm5 %02X", instr->operands[opd].imm5);
            break;
        case CPU_OPD_IMM8:
            fprintf(stderr, "imm8 %02X", instr->operands[opd].u8);
            break;
        case CPU_OPD_IMM32:
            fprintf(stderr, "imm32 %08X", instr->operands[opd].u32);
            break;
        }
        if (opd == instr->desc->num_operands - 1) {
            fprintf(stderr, "]");
        } else {
            fprintf(stderr, ", ");
        }
    }
    fprintf(stderr, "\n");
}

static bool prv_cpu_check_err(cpu_ctx_t *cpu, vm_err_t err) {
    D_ASSERT(cpu);
    if (err != VM_ERR_NONE) {
        prv_cpu_raise_exception(cpu, err);
        return true;
    } else {
        return false;
    }
}

static void prv_cpu_raise_exception(cpu_ctx_t *cpu, vm_err_t err) {
    D_ASSERT(cpu);
    cpu->state = CPU_INT_FETCH_ISR_ADDR;

    uint8_t exc_num = (uint8_t)cpu_exc_type_of_err(cpu, err);

    cpu->num_nested_exc++;
    cpu->curr_int_line = exc_num;
    cpu->pc_after_isr = cpu->instr.start_addr;

    D_PRINTF("exception %u, count %zu, pc = 0x%08X", exc_num,
             cpu->num_nested_exc, cpu->reg_pc);

    if (cpu->num_nested_exc == 3) {
        cpu->state = CPU_TRIPLE_FAULT;
        return;
    }
}
