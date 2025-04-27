#include <limits.h>
#include <string.h>

#include "cpu.h"
#include "debugm.h"

static uint8_t prv_cpu_fetch_u8(vm_state_t *vm);
static uint32_t prv_cpu_fetch_u32(vm_state_t *vm);
static void prv_cpu_fetch_reg(vm_state_t *vm, uint8_t *p_reg);
static void prv_cpu_fetch_regs(vm_state_t *vm, uint8_t *p_reg_src,
                               uint8_t *p_reg_dst);

static uint32_t *prv_cpu_decode_reg(vm_state_t *vm, uint8_t reg);

static void prv_cpu_decode_execute(vm_state_t *vm);
static bool prv_cpu_decode_execute_data(vm_state_t *vm, uint8_t opcode);
static bool prv_cpu_decode_execute_alu(vm_state_t *vm, uint8_t opcode);

static void prv_cpu_set_flags(vm_state_t *vm, bool zero, bool sign, bool carry,
                              bool overflow);

void cpu_init(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    cpu_reset(vm);
}

void cpu_deinit(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    // Do nothing.
}

void cpu_reset(vm_state_t *vm) {
    D_ASSERT(vm != NULL);

    // Clear all registers (except PC).
    memset(vm->regs_gp, 0, sizeof(vm->regs_gp));
    vm->reg_sp = 0;
    vm->flags = 0;
    vm->cycles = 0;
    vm->interrupt_depth = 0;

    // Jump to the reset handler.
    uint32_t reset_isr_addr = vm_read_u32(vm, 0);
    vm->reg_pc = reset_isr_addr;
}

void cpu_step(vm_state_t *vm) {
    D_ASSERT(vm != NULL);

    prv_cpu_decode_execute(vm);

    vm->cycles++;
}

static uint8_t prv_cpu_fetch_u8(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    uint8_t byte = vm_read_u8(vm, vm->reg_pc);
    vm->reg_pc += 1;
    return byte;
}

static uint32_t prv_cpu_fetch_u32(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    uint32_t dword = vm_read_u32(vm, vm->reg_pc);
    vm->reg_pc += 4;
    return dword;
}

static void prv_cpu_fetch_reg(vm_state_t *vm, uint8_t *p_reg) {
    D_ASSERT(vm != NULL);
    D_ASSERT(p_reg != NULL);
    uint8_t byte = prv_cpu_fetch_u8(vm);
    *p_reg = byte;
}

static void prv_cpu_fetch_regs(vm_state_t *vm, uint8_t *p_reg_src,
                               uint8_t *p_reg_dst) {
    D_ASSERT(vm != NULL);
    D_ASSERT(p_reg_src != NULL);
    D_ASSERT(p_reg_dst != NULL);
    uint8_t byte = prv_cpu_fetch_u8(vm);
    *p_reg_src = (byte >> 4) & 0x0F;
    *p_reg_dst = (byte >> 0) & 0x0F;
}

static uint32_t *prv_cpu_decode_reg(vm_state_t *vm, uint8_t reg) {
    D_ASSERT(vm != NULL);
    uint32_t *code_addr_map[0xFF] = {
        [CPU_CODE_R0] = &vm->regs_gp[0], [CPU_CODE_R1] = &vm->regs_gp[1],
        [CPU_CODE_R2] = &vm->regs_gp[2], [CPU_CODE_R3] = &vm->regs_gp[3],
        [CPU_CODE_R4] = &vm->regs_gp[4], [CPU_CODE_R5] = &vm->regs_gp[5],
        [CPU_CODE_R6] = &vm->regs_gp[6], [CPU_CODE_R7] = &vm->regs_gp[7],
        [CPU_CODE_SP] = &vm->reg_sp,
    };
    // TODO: raise an exception
    uint32_t *p_vm_reg = code_addr_map[reg];
    D_ASSERTMF(p_vm_reg != NULL, "invalid register code: 0x%02X", reg);
    return p_vm_reg;
}

static void prv_cpu_decode_execute(vm_state_t *vm) {
    D_ASSERT(vm != NULL);

    uint8_t opcode = prv_cpu_fetch_u8(vm);
    uint8_t opcode_kind = opcode & CPU_OP_KIND_MASK;
    bool ok = false;
    if (opcode_kind == CPU_OP_KIND_DATA) {
        ok = prv_cpu_decode_execute_data(vm, opcode);
    } else if (opcode_kind == CPU_OP_KIND_ALU) {
        ok = prv_cpu_decode_execute_alu(vm, opcode);
    }

    D_ASSERTM(ok, "invalid opcode");
}

static bool prv_cpu_decode_execute_data(vm_state_t *vm, uint8_t opcode) {
    switch (opcode) {
        // Data movement
    case CPU_OP_MOV_RR: {
        uint8_t c_reg_src;
        uint8_t c_reg_dst;
        prv_cpu_fetch_regs(vm, &c_reg_src, &c_reg_dst);

        uint32_t *p_vm_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        uint32_t *p_vm_reg_dst = prv_cpu_decode_reg(vm, c_reg_dst);

        D_ASSERT(p_vm_reg_src != NULL);
        D_ASSERT(p_vm_reg_dst != NULL);
        *p_vm_reg_dst = *p_vm_reg_src;
        break;
    }
    case CPU_OP_MOV_VR: {
        uint8_t c_reg_dst;
        prv_cpu_fetch_reg(vm, &c_reg_dst);
        uint32_t dword = prv_cpu_fetch_u32(vm);

        uint32_t *p_vm_reg_dst = prv_cpu_decode_reg(vm, c_reg_dst);
        D_ASSERT(p_vm_reg_dst != NULL);
        *p_vm_reg_dst = dword;
        break;
    }

    case CPU_OP_STR_RI0: {
        uint8_t c_reg_src;
        uint8_t c_reg_dst_mem;
        prv_cpu_fetch_regs(vm, &c_reg_src, &c_reg_dst_mem);

        uint32_t *p_vm_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        uint32_t *p_vm_reg_dst_mem = prv_cpu_decode_reg(vm, c_reg_dst_mem);

        D_ASSERT(p_vm_reg_src != NULL);
        D_ASSERT(p_vm_reg_dst_mem != NULL);
        uint32_t dword = *p_vm_reg_src;
        uint32_t dest_mem = *p_vm_reg_dst_mem;
        vm_write_u32(vm, dest_mem, dword);
        break;
    }
    case CPU_OP_STR_RV0: {
        uint8_t c_reg_src;
        prv_cpu_fetch_reg(vm, &c_reg_src);
        uint32_t dest_mem = prv_cpu_fetch_u32(vm);

        uint32_t *p_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        D_ASSERT(p_reg_src != NULL);
        uint32_t dword = *p_reg_src;
        vm_write_u32(vm, dest_mem, dword);
        break;
    }
    case CPU_OP_STR_RI8: {
        uint8_t c_reg_src;
        uint8_t c_reg_dst_mem;
        prv_cpu_fetch_regs(vm, &c_reg_src, &c_reg_dst_mem);
        int8_t offset = (int8_t)prv_cpu_fetch_u8(vm);

        uint32_t *p_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        uint32_t *p_reg_dst_mem = prv_cpu_decode_reg(vm, c_reg_dst_mem);
        D_ASSERT(p_reg_src != NULL);
        D_ASSERT(p_reg_dst_mem != NULL);

        uint32_t dword = *p_reg_src;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_write_u32(vm, dest_mem, dword);
        break;
    }
    case CPU_OP_STR_RI32: {
        uint8_t c_reg_src;
        uint8_t c_reg_dst_mem;
        prv_cpu_fetch_regs(vm, &c_reg_src, &c_reg_dst_mem);
        int32_t offset = (int32_t)prv_cpu_fetch_u32(vm);

        uint32_t *p_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        uint32_t *p_reg_dst_mem = prv_cpu_decode_reg(vm, c_reg_dst_mem);
        D_ASSERT(p_reg_src != NULL);
        D_ASSERT(p_reg_dst_mem != NULL);

        uint32_t dword = *p_reg_src;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_write_u32(vm, dest_mem, dword);
        break;
    }
    case CPU_OP_STR_RIR: {
        uint8_t c_reg_src;
        uint8_t c_reg_dst_mem;
        prv_cpu_fetch_regs(vm, &c_reg_src, &c_reg_dst_mem);
        uint8_t c_reg_off;
        prv_cpu_fetch_reg(vm, &c_reg_off);

        uint32_t *p_reg_src = prv_cpu_decode_reg(vm, c_reg_src);
        uint32_t *p_reg_dst_mem = prv_cpu_decode_reg(vm, c_reg_dst_mem);
        uint32_t *p_reg_off = prv_cpu_decode_reg(vm, c_reg_off);
        D_ASSERT(p_reg_src != NULL);
        D_ASSERT(p_reg_dst_mem != NULL);
        D_ASSERT(p_reg_off != NULL);

        uint32_t dword = *p_reg_src;
        int32_t offset = *p_reg_off;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_write_u32(vm, dest_mem, dword);
        break;
    }

    case CPU_OP_LDR_RV0: {
        uint8_t c_reg_dst;
        prv_cpu_fetch_reg(vm, &c_reg_dst);
        uint32_t mem_addr = prv_cpu_fetch_u32(vm);

        uint32_t *p_reg_dst = prv_cpu_decode_reg(vm, c_reg_dst);
        D_ASSERT(p_reg_dst != NULL);

        *p_reg_dst = vm_read_u32(vm, mem_addr);
        break;
    }
    case CPU_OP_LDR_RI0:
    case CPU_OP_LDR_RI8:
    case CPU_OP_LDR_RI32:
    case CPU_OP_LDR_RIR: {
        uint8_t c_reg_src_mem;
        uint8_t c_reg_dst;
        prv_cpu_fetch_regs(vm, &c_reg_src_mem, &c_reg_dst);

        uint32_t *p_reg_src_mem = prv_cpu_decode_reg(vm, c_reg_src_mem);
        uint32_t *p_reg_dst = prv_cpu_decode_reg(vm, c_reg_dst);
        D_ASSERT(p_reg_src_mem != NULL);
        D_ASSERT(p_reg_dst != NULL);

        int32_t offset = 0;
        if (opcode == CPU_OP_LDR_RI8) {
            offset = (int8_t)prv_cpu_fetch_u8(vm);
        } else if (opcode == CPU_OP_LDR_RI32) {
            offset = (int32_t)prv_cpu_fetch_u32(vm);
        } else if (opcode == CPU_OP_LDR_RIR) {
            uint8_t c_reg_off;
            prv_cpu_fetch_reg(vm, &c_reg_off);
            uint32_t *p_reg_off = prv_cpu_decode_reg(vm, c_reg_off);
            D_ASSERT(p_reg_off != NULL);
            offset = (int32_t)*p_reg_off;
        }

        uint32_t src_mem = *p_reg_src_mem + offset;
        uint32_t dword = vm_read_u32(vm, src_mem);
        *p_reg_dst = dword;
        break;
    }

    default:
        return false;
    }
    return true;
}

static bool prv_cpu_decode_execute_alu(vm_state_t *vm, uint8_t opcode) {
    uint32_t *p_reg = NULL;
    uint32_t op_val;

    // Fetch and decode the operands.
    if ((opcode & 1) == 0) {
        // Even opcodes require two register operands.
        uint8_t c_reg_dst;
        uint8_t c_reg_src;
        prv_cpu_fetch_regs(vm, &c_reg_dst, &c_reg_src);

        p_reg = prv_cpu_decode_reg(vm, c_reg_dst);
        uint32_t *p_reg_src = prv_cpu_decode_reg(vm, c_reg_src);

        D_ASSERT(p_reg_src != NULL);
        op_val = *p_reg_src;
    } else if (opcode == CPU_OP_NOT_R) {
        // Only one register operand.
        uint8_t c_reg_dst;
        prv_cpu_fetch_reg(vm, &c_reg_dst);
        p_reg = prv_cpu_decode_reg(vm, c_reg_dst);
        D_ASSERT(p_reg != NULL);
    } else {
        // Odd opcodes require a register operand and an imm32 value, except for
        // some opcodes.
        uint8_t c_reg_dst;
        prv_cpu_fetch_reg(vm, &c_reg_dst);
        p_reg = prv_cpu_decode_reg(vm, c_reg_dst);
        D_ASSERT(p_reg != NULL);

        if (opcode == CPU_OP_SHL_RV || opcode == CPU_OP_SHR_RV ||
            opcode == CPU_OP_ROL_RV || opcode == CPU_OP_ROR_RV) {
            // Second operand is an imm5.
            uint8_t op2 = prv_cpu_fetch_u8(vm);
            D_ASSERTM((op2 & ~31) == 0x00, "garbage bits in an imm5 value");
            op_val = (uint32_t)op2;
        } else {
            // Second operand is an imm32.
            uint32_t op2 = prv_cpu_fetch_u32(vm);
            op_val = op2;
        }
    }

    // Execute the instruction.
    bool flag_zero = false;
    bool flag_sign = false;
    bool flag_carry = false;
    bool flag_ovf = false;
    D_ASSERT(p_reg != NULL);
    switch (opcode) {
    case CPU_OP_ADD_RR:
    case CPU_OP_ADD_RV: {
        uint64_t res = (uint64_t)*p_reg + op_val;
        bool sign_op1 = (*p_reg & (1 << 31)) != 0;
        bool sign_op2 = (op_val & (1 << 31)) != 0;
        bool sign_res = (res & (1 << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = (res & ~0xFFFFFFFFL) != 0;
        flag_ovf = (sign_op1 == sign_op2) && (sign_res != sign_op1);
        *p_reg = (uint32_t)res;
    } break;
    case CPU_OP_SUB_RR:
    case CPU_OP_SUB_RV: {
        uint32_t res = *p_reg - op_val;
        bool sign_op1 = (*p_reg & (1 << 31)) != 0;
        bool sign_op2 = (op_val & (1 << 31)) != 0;
        bool sign_res = (res & (1 << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = *p_reg >= op_val;
        flag_ovf = (sign_op1 != sign_op2) && (sign_res != sign_op1);
        *p_reg = res;
        break;
    }
    case CPU_OP_MUL_RR:
    case CPU_OP_MUL_RV: {
        uint64_t res = (uint64_t)*p_reg * op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = (res & ~0xFFFFFFFFL) != 0;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_DIV_RR:
    case CPU_OP_DIV_RV: {
        D_ASSERT(op_val != 0);
        uint32_t res = *p_reg / op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg /= op_val;
        break;
    }
    case CPU_OP_IDIV_RR:
    case CPU_OP_IDIV_RV: {
        D_ASSERT(!((int32_t)*p_reg == INT_MIN && (int32_t)op_val == -1));
        int32_t res = (int32_t)*p_reg / (int32_t)op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_AND_RR:
    case CPU_OP_AND_RV: {
        uint32_t res = *p_reg & op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_OR_RR:
    case CPU_OP_OR_RV: {
        uint32_t res = *p_reg | op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_XOR_RR:
    case CPU_OP_XOR_RV: {
        uint32_t res = *p_reg ^ op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_NOT_R: {
        uint32_t res = ~(*p_reg);
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_SHL_RR:
    case CPU_OP_SHL_RV: {
        uint32_t numbits = op_val & 31;
        uint32_t res = *p_reg << numbits;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = (*p_reg & (1 << (32 - numbits))) != 0;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_SHR_RR:
    case CPU_OP_SHR_RV: {
        uint32_t numbits = op_val & 31;
        uint32_t res = *p_reg >> numbits;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = (*p_reg & (1 << (-1 + numbits))) != 0;
        *p_reg = (uint32_t)res;
        break;
    }
    case CPU_OP_ROL_RR:
    case CPU_OP_ROL_RV:
        D_TODO();
        break;
    case CPU_OP_ROR_RR:
    case CPU_OP_ROR_RV:
        D_TODO();
        break;
    case CPU_OP_CMP_RR: {
        uint32_t res = *p_reg - op_val;
        bool sign_op1 = (*p_reg & (1 << 31)) != 0;
        bool sign_op2 = (op_val & (1 << 31)) != 0;
        bool sign_res = (res & (1 << 31)) != 0;
        flag_zero = res == 0;
        flag_sign = sign_res;
        flag_carry = *p_reg >= op_val;
        flag_ovf = (sign_op1 != sign_op2) && (sign_res != sign_op1);
        break;
    }
    case CPU_OP_TST_RR:
    case CPU_OP_TST_RV: {
        uint32_t res = *p_reg & op_val;
        flag_zero = res == 0;
        flag_sign = (res & (1 << 31)) != 0;
        flag_carry = false;
        break;
    }
    default:
        return false;
    }

    prv_cpu_set_flags(vm, flag_zero, flag_sign, flag_carry, flag_ovf);
    return true;
}

static void prv_cpu_set_flags(vm_state_t *vm, bool zero, bool sign, bool carry,
                              bool overflow) {
    D_ASSERT(vm != NULL);
    if (zero) {
        vm->flags |= CPU_FLAG_ZERO;
    } else {
        vm->flags &= ~CPU_FLAG_ZERO;
    }
    if (sign) {
        vm->flags |= CPU_FLAG_SIGN;
    } else {
        vm->flags &= ~CPU_FLAG_SIGN;
    }
    if (carry) {
        vm->flags |= CPU_FLAG_CARRY;
    } else {
        vm->flags &= ~CPU_FLAG_CARRY;
    }
    if (overflow) {
        vm->flags |= CPU_FLAG_OVERFLOW;
    } else {
        vm->flags &= ~CPU_FLAG_OVERFLOW;
    }
}
