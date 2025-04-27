#include <limits.h>
#include <string.h>

#include "cpu.h"
#include "debugm.h"
#include "mem.h"

#define FETCH_U8(TYPE_CAST, OUT_VAL)                                           \
    TYPE_CAST OUT_VAL;                                                         \
    {                                                                          \
        vm_res_u8_t _imm8_res = prv_cpu_fetch_u8(vm);                          \
        if (!_imm8_res.ok) {                                                   \
            vm_res_t _res = {.ok = false, .exc = _imm8_res.exc};               \
            return _res;                                                       \
        }                                                                      \
        OUT_VAL = (TYPE_CAST)_imm8_res.byte;                                   \
    }
#define FETCH_U32(TYPE_CAST, OUT_VAL)                                          \
    TYPE_CAST OUT_VAL;                                                         \
    {                                                                          \
        vm_res_u32_t _imm32_res = prv_cpu_fetch_u32(vm);                       \
        if (!_imm32_res.ok) {                                                  \
            vm_res_t _res = {.ok = false, .exc = _imm32_res.exc};              \
            return _res;                                                       \
        }                                                                      \
        OUT_VAL = (TYPE_CAST)_imm32_res.dword;                                 \
    }

#define FETCH_REG(OUT_A)                                                       \
    uint8_t OUT_A;                                                             \
    {                                                                          \
        vm_res_t _reg_res = prv_cpu_fetch_reg(vm, &OUT_A);                     \
        if (!_reg_res.ok) { return _reg_res; }                                 \
    }
#define FETCH_REGS(OUT_A, OUT_B)                                               \
    uint8_t OUT_A;                                                             \
    uint8_t OUT_B;                                                             \
    {                                                                          \
        vm_res_t _reg_res = prv_cpu_fetch_regs(vm, &OUT_A, &OUT_B);            \
        if (!_reg_res.ok) { return _reg_res; }                                 \
    }

#define DECODE_REG(OUT_REG_PTR, IN_REG_CODE)                                   \
    uint32_t *OUT_REG_PTR;                                                     \
    {                                                                          \
        vm_res_pu32_t _dec_res = prv_cpu_decode_reg(vm, IN_REG_CODE);          \
        if (!_dec_res.ok) {                                                    \
            vm_res_t _res = {.ok = false, .exc = _dec_res.exc};                \
            return _res;                                                       \
        }                                                                      \
        OUT_REG_PTR = _dec_res.ptr;                                            \
    }                                                                          \
    D_ASSERT(OUT_REG_PTR != NULL);

static vm_res_u8_t prv_cpu_fetch_u8(vm_state_t *vm);
static vm_res_u32_t prv_cpu_fetch_u32(vm_state_t *vm);
static vm_res_t prv_cpu_fetch_reg(vm_state_t *vm, uint8_t *p_reg);
static vm_res_t prv_cpu_fetch_regs(vm_state_t *vm, uint8_t *p_reg_src,
                                   uint8_t *p_reg_dst);

static vm_res_t prv_cpu_stack_push_u32(vm_state_t *vm, uint32_t val);
static vm_res_u32_t prv_cpu_stack_pop_u32(vm_state_t *vm);

static vm_res_pu32_t prv_cpu_decode_reg(vm_state_t *vm, uint8_t reg);

static vm_res_t prv_cpu_decode_execute(vm_state_t *vm);
static vm_res_t prv_cpu_decode_execute_data(vm_state_t *vm, uint8_t opcode);
static vm_res_t prv_cpu_decode_execute_alu(vm_state_t *vm, uint8_t opcode);
static vm_res_t prv_cpu_decode_execute_flow(vm_state_t *vm, uint8_t opcode);
static vm_res_t prv_cpu_decode_execute_stack(vm_state_t *vm, uint8_t opcode);
static vm_res_t prv_cpu_decode_execute_other(vm_state_t *vm, uint8_t opcode);

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
    vm_res_u32_t reset_isr_addr = mem_read_u32(vm, 0);
    D_ASSERT(reset_isr_addr.ok == true);
    vm->reg_pc = reset_isr_addr.dword;
}

void cpu_step(vm_state_t *vm) {
    D_ASSERT(vm != NULL);

    prv_cpu_decode_execute(vm);

    vm->cycles++;
}

static vm_res_u8_t prv_cpu_fetch_u8(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    vm_res_u8_t res = mem_read_u8(vm, vm->reg_pc);
    if (res.ok) { vm->reg_pc += 1; }
    return res;
}

static vm_res_u32_t prv_cpu_fetch_u32(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    vm_res_u32_t res = mem_read_u32(vm, vm->reg_pc);
    if (res.ok) { vm->reg_pc += 4; }
    return res;
}

static vm_res_t prv_cpu_fetch_reg(vm_state_t *vm, uint8_t *p_reg) {
    D_ASSERT(vm != NULL);
    D_ASSERT(p_reg != NULL);
    vm_res_u8_t in_res = prv_cpu_fetch_u8(vm);
    if (in_res.ok) { *p_reg = in_res.byte; }
    vm_res_t out_res = {.ok = in_res.ok, .exc = in_res.exc};
    return out_res;
}

static vm_res_t prv_cpu_fetch_regs(vm_state_t *vm, uint8_t *p_reg_src,
                                   uint8_t *p_reg_dst) {
    D_ASSERT(vm != NULL);
    D_ASSERT(p_reg_src != NULL);
    D_ASSERT(p_reg_dst != NULL);
    vm_res_u8_t in_res = prv_cpu_fetch_u8(vm);
    if (in_res.ok) {
        *p_reg_src = (in_res.byte >> 4) & 0x0F;
        *p_reg_dst = (in_res.byte >> 0) & 0x0F;
    }
    vm_res_t out_res = {.ok = in_res.ok, .exc = in_res.exc};
    return out_res;
}

static vm_res_t prv_cpu_stack_push_u32(vm_state_t *vm, uint32_t val) {
    D_ASSERT(vm != NULL);
    if (vm->reg_sp >= 4) {
        vm->reg_sp -= 4;
        return mem_write_u32(vm, vm->reg_sp, val);
    } else {
        vm_res_t res = {.ok = false, .exc = {.type = VM_EXC_STACK_OVERFLOW}};
        return res;
    }
}

static vm_res_u32_t prv_cpu_stack_pop_u32(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    D_ASSERT(vm->reg_sp >= 4);
    vm_res_u32_t res = mem_read_u32(vm, vm->reg_sp);
    if (res.ok) {
        if (vm->reg_sp <= 0xFFFFFFFF - 4) {
            vm->reg_sp += 4;
        } else {
            res.ok = false;
            res.exc.type = VM_EXC_STACK_OVERFLOW;
        }
    }
    return res;
}

static vm_res_pu32_t prv_cpu_decode_reg(vm_state_t *vm, uint8_t reg) {
    D_ASSERT(vm != NULL);
    vm_res_pu32_t res = {.ok = true};
    uint32_t *code_addr_map[0xFF] = {
        [CPU_CODE_R0] = &vm->regs_gp[0], [CPU_CODE_R1] = &vm->regs_gp[1],
        [CPU_CODE_R2] = &vm->regs_gp[2], [CPU_CODE_R3] = &vm->regs_gp[3],
        [CPU_CODE_R4] = &vm->regs_gp[4], [CPU_CODE_R5] = &vm->regs_gp[5],
        [CPU_CODE_R6] = &vm->regs_gp[6], [CPU_CODE_R7] = &vm->regs_gp[7],
        [CPU_CODE_SP] = &vm->reg_sp,
    };
    res.ptr = code_addr_map[reg];
    if (res.ptr == NULL) {
        D_PRINTF("invalid register code: 0x%02X", reg);
        res.ok = false;
        res.exc.type = VM_EXC_BAD_OPCODE;
    }
    return res;
}

static vm_res_t prv_cpu_decode_execute(vm_state_t *vm) {
    D_ASSERT(vm != NULL);
    vm_res_t res = {.ok = true};

    FETCH_U8(uint8_t, opcode);
    uint8_t opcode_kind = opcode & CPU_OP_KIND_MASK;
    if (opcode_kind == CPU_OP_KIND_DATA) {
        res = prv_cpu_decode_execute_data(vm, opcode);
    } else if (opcode_kind == CPU_OP_KIND_ALU) {
        res = prv_cpu_decode_execute_alu(vm, opcode);
    } else if (opcode_kind == CPU_OP_KIND_FLOW) {
        res = prv_cpu_decode_execute_flow(vm, opcode);
    } else if (opcode_kind == CPU_OP_KIND_STACK) {
        res = prv_cpu_decode_execute_stack(vm, opcode);
    } else if (opcode_kind == CPU_OP_KIND_OTHER) {
        res = prv_cpu_decode_execute_other(vm, opcode);
    }

    return res;
}

static vm_res_t prv_cpu_decode_execute_data(vm_state_t *vm, uint8_t opcode) {
    switch (opcode) {
        // Data movement
    case CPU_OP_MOV_RR: {
        FETCH_REGS(c_reg_src, c_reg_dst);
        DECODE_REG(p_reg_src, c_reg_src);
        DECODE_REG(p_reg_dst, c_reg_dst);
        *p_reg_dst = *p_reg_src;
        break;
    }
    case CPU_OP_MOV_VR: {
        FETCH_REG(c_reg_dst);
        FETCH_U32(uint32_t, dword);
        DECODE_REG(p_reg_dst, c_reg_dst);
        *p_reg_dst = dword;
        break;
    }

    case CPU_OP_STR_RI0: {
        FETCH_REGS(c_reg_src, c_reg_dst_mem);
        DECODE_REG(p_reg_src, c_reg_src);
        DECODE_REG(p_reg_dst_mem, c_reg_dst_mem);
        uint32_t dword = *p_reg_src;
        uint32_t dest_mem = *p_reg_dst_mem;
        vm_res_t res = mem_write_u32(vm, dest_mem, dword);
        if (!res.ok) { return res; }
        break;
    }
    case CPU_OP_STR_RV0: {
        FETCH_REG(c_reg_src);
        FETCH_U32(uint32_t, dest_mem);
        DECODE_REG(p_reg_src, c_reg_src);
        uint32_t dword = *p_reg_src;
        vm_res_t res = mem_write_u32(vm, dest_mem, dword);
        if (!res.ok) { return res; }
        break;
    }
    case CPU_OP_STR_RI8: {
        FETCH_REGS(c_reg_src, c_reg_dst_mem);
        FETCH_U8(int8_t, offset);
        DECODE_REG(p_reg_src, c_reg_src);
        DECODE_REG(p_reg_dst_mem, c_reg_dst_mem);
        uint32_t dword = *p_reg_src;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_res_t res = mem_write_u32(vm, dest_mem, dword);
        if (!res.ok) { return res; }
        break;
    }
    case CPU_OP_STR_RI32: {
        FETCH_REGS(c_reg_src, c_reg_dst_mem);
        FETCH_U32(int32_t, offset);
        DECODE_REG(p_reg_src, c_reg_src);
        DECODE_REG(p_reg_dst_mem, c_reg_dst_mem);
        uint32_t dword = *p_reg_src;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_res_t res = mem_write_u32(vm, dest_mem, dword);
        if (!res.ok) { return res; }
        break;
    }
    case CPU_OP_STR_RIR: {
        FETCH_REGS(c_reg_src, c_reg_dst_mem);
        FETCH_REG(c_reg_off);
        DECODE_REG(p_reg_src, c_reg_src);
        DECODE_REG(p_reg_dst_mem, c_reg_dst_mem);
        DECODE_REG(p_reg_off, c_reg_off);
        uint32_t dword = *p_reg_src;
        int32_t offset = *p_reg_off;
        uint32_t dest_mem = *p_reg_dst_mem + offset;
        vm_res_t res = mem_write_u32(vm, dest_mem, dword);
        if (!res.ok) { return res; }
        break;
    }

    case CPU_OP_LDR_RV0: {
        FETCH_REG(c_reg_dst);
        FETCH_U32(uint32_t, mem_addr);
        DECODE_REG(p_reg_dst, c_reg_dst);
        vm_res_u32_t mem_res = mem_read_u32(vm, mem_addr);
        if (mem_res.ok) {
            *p_reg_dst = mem_res.dword;
        } else {
            vm_res_t res = {.ok = false, .exc = mem_res.exc};
            return res;
        }
        break;
    }
    case CPU_OP_LDR_RI0:
    case CPU_OP_LDR_RI8:
    case CPU_OP_LDR_RI32:
    case CPU_OP_LDR_RIR: {
        FETCH_REGS(c_reg_src_mem, c_reg_dst);
        DECODE_REG(p_reg_src_mem, c_reg_src_mem);
        DECODE_REG(p_reg_dst, c_reg_dst);

        int32_t offset = 0;
        if (opcode == CPU_OP_LDR_RI8) {
            FETCH_U8(int8_t, by_offset);
            offset = (int32_t)by_offset;
        } else if (opcode == CPU_OP_LDR_RI32) {
            FETCH_U32(int32_t, dw_offset);
            offset = dw_offset;
        } else if (opcode == CPU_OP_LDR_RIR) {
            FETCH_REG(c_reg_off);
            DECODE_REG(p_reg_off, c_reg_off);
            offset = (int32_t)*p_reg_off;
        }

        uint32_t src_mem = *p_reg_src_mem + offset;
        vm_res_u32_t mem_res = mem_read_u32(vm, src_mem);
        if (mem_res.ok) {
            *p_reg_dst = mem_res.dword;
        } else {
            vm_res_t res = {.ok = false, .exc = mem_res.exc};
            return res;
        }
        break;
    }

    default: {
        vm_res_t res = {};
        res.ok = false;
        res.exc.type = VM_EXC_BAD_OPCODE;
        return res;
    }
    }
    vm_res_t res = {};
    res.ok = true;
    return res;
}

static vm_res_t prv_cpu_decode_execute_alu(vm_state_t *vm, uint8_t opcode) {
    uint32_t *p_reg = NULL;
    uint32_t op_val;

    // Fetch and decode the operands.
    if ((opcode & 1) == 0) {
        // Even opcodes require two register operands.
        FETCH_REGS(c_reg_dst, c_reg_src);
        DECODE_REG(p_reg_dst, c_reg_dst);
        DECODE_REG(p_reg_src, c_reg_src);
        p_reg = p_reg_dst;
        op_val = *p_reg_src;
    } else if (opcode == CPU_OP_NOT_R) {
        // Only one register operand.
        FETCH_REG(c_reg_dst);
        DECODE_REG(p_reg_dst, c_reg_dst);
        p_reg = p_reg_dst;
    } else {
        // Odd opcodes require a register operand and an imm32 value, except for
        // some opcodes.
        FETCH_REG(c_reg_dst);
        DECODE_REG(p_reg_dst, c_reg_dst);
        p_reg = p_reg_dst;

        if (opcode == CPU_OP_SHL_RV || opcode == CPU_OP_SHR_RV ||
            opcode == CPU_OP_ROL_RV || opcode == CPU_OP_ROR_RV) {
            // Second operand is an imm5.
            FETCH_U8(uint8_t, op2);
            if ((op2 & ~31) != 0x00) {
                D_PRINT("garbage bits in an imm5 value");
                vm_res_t res = {};
                res.ok = false;
                res.exc.type = VM_EXC_BAD_OPCODE;
                return res;
            }
            op_val = (uint32_t)op2;
        } else {
            // Second operand is an imm32.
            FETCH_U8(uint32_t, op2);
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
    default: {
        vm_res_t res = {};
        res.ok = false;
        res.exc.type = VM_EXC_BAD_OPCODE;
        return res;
    }
    }

    prv_cpu_set_flags(vm, flag_zero, flag_sign, flag_carry, flag_ovf);
    vm_res_t res = {.ok = true};
    return res;
}

static vm_res_t prv_cpu_decode_execute_flow(vm_state_t *vm, uint8_t opcode) {
    uint32_t instr_addr = vm->reg_pc - 1;
    uint32_t jump_addr = 0;
    if (opcode != CPU_OP_RET) {
        uint8_t opcode_type = opcode & 0b11;
        if (opcode_type == 0b00) { // imm8
            FETCH_U8(int8_t, offset);
            jump_addr = instr_addr + offset;
        } else if (opcode_type == 0b01) { // imm32
            FETCH_U32(uint32_t, abs_addr);
            jump_addr = abs_addr;
        } else if (opcode_type == 0b10) { // register
            FETCH_REG(c_reg);
            DECODE_REG(p_reg, c_reg);
            jump_addr = *p_reg;
        } else {
            vm_res_t res = {.ok = false, .exc.type = VM_EXC_BAD_OPCODE};
            return res;
        }
    }

    bool flag_zero = (vm->flags & CPU_FLAG_ZERO) != 0;
    bool flag_S_xor_V = (((vm->flags & CPU_FLAG_SIGN) != 0) ^
                         ((vm->flags & CPU_FLAG_OVERFLOW) != 0));
    switch (opcode) {
    case CPU_OP_JMPR_V8:
    case CPU_OP_JMPA_V32:
    case CPU_OP_JMPA_R:
        vm->reg_pc = jump_addr;
        break;
    case CPU_OP_JEQR_V8:
    case CPU_OP_JEQA_V32:
    case CPU_OP_JEQA_R:
        if (flag_zero) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_JNER_V8:
    case CPU_OP_JNEA_V32:
    case CPU_OP_JNEA_R:
        if (!flag_zero) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_JGTR_V8:
    case CPU_OP_JGTA_V32:
    case CPU_OP_JGTA_R:
        if (!flag_zero && !flag_S_xor_V) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_JGER_V8:
    case CPU_OP_JGEA_V32:
    case CPU_OP_JGEA_R:
        if (!flag_S_xor_V) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_JLTR_V8:
    case CPU_OP_JLTA_V32:
    case CPU_OP_JLTA_R:
        if (flag_S_xor_V) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_JLER_V8:
    case CPU_OP_JLEA_V32:
    case CPU_OP_JLEA_R:
        if (flag_zero || flag_S_xor_V) { vm->reg_pc = jump_addr; }
        break;
    case CPU_OP_CALLA_V32: {
        uint32_t instr_size = 5;
        prv_cpu_stack_push_u32(vm, instr_addr + instr_size);
        vm->reg_pc = jump_addr;
    } break;
    case CPU_OP_CALLA_R: {
        uint32_t instr_size = 2;
        prv_cpu_stack_push_u32(vm, instr_addr + instr_size);
        vm->reg_pc = jump_addr;
        break;
    }
    case CPU_OP_RET: {
        vm_res_u32_t stack_res = prv_cpu_stack_pop_u32(vm);
        if (stack_res.ok) {
            jump_addr = stack_res.dword;
        } else {
            vm_res_t res = {};
            res.ok = false;
            res.exc = stack_res.exc;
            return res;
        }
        vm->reg_pc = jump_addr;
        break;
    }
    default: {
        vm_res_t res = {};
        res.ok = false;
        res.exc.type = VM_EXC_BAD_OPCODE;
        return res;
    }
    }
    vm_res_t res = {.ok = true};
    return res;
}

static vm_res_t prv_cpu_decode_execute_stack(vm_state_t *vm, uint8_t opcode) {
    D_ASSERT(vm != NULL);
    if (opcode == CPU_OP_PUSH_V32 || opcode == CPU_OP_PUSH_R) {
        uint32_t val;
        if (opcode == CPU_OP_PUSH_V32) {
            FETCH_U32(uint32_t, imm32);
            val = imm32;
        } else {
            FETCH_REG(c_reg);
            DECODE_REG(p_reg, c_reg);
            val = *p_reg;
        }
        prv_cpu_stack_push_u32(vm, val);
    } else if (opcode == CPU_OP_POP_R) {
        FETCH_REG(c_reg);
        DECODE_REG(p_reg, c_reg);
        vm_res_u32_t stack_res = prv_cpu_stack_pop_u32(vm);
        if (stack_res.ok) {
            *p_reg = stack_res.dword;
        } else {
            vm_res_t res = {.ok = false, .exc = stack_res.exc};
            return res;
        }
    } else {
        vm_res_t res = {.ok = false, .exc.type = VM_EXC_BAD_OPCODE};
        return res;
    }
    vm_res_t res = {.ok = true};
    return res;
}

static vm_res_t prv_cpu_decode_execute_other(vm_state_t *vm, uint8_t opcode) {
    if (opcode == CPU_OP_NOP) {
    } else if (opcode == CPU_OP_HALT) {
        D_TODO();
    } else if (opcode == CPU_OP_INT_V8) {
        FETCH_U8(uint8_t, int_num);
        D_PRINTF("INT 0x%02X", int_num);
        D_TODO();
    } else {
        vm_res_t res = {.ok = false, .exc.type = VM_EXC_BAD_OPCODE};
        return res;
    }
    vm_res_t res = {.ok = true};
    return res;
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
