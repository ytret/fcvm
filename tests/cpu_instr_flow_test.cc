#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_MEM_SIZE 100

struct FlowInstrParam {
    enum AddrType {
        RelToPC,     //!< Jump address is stored as an IMM8 offset of PC.
        AddrInIMM32, //!< Jump address is stored as an IMM32.
        AddrInReg,   //!< Jump address is stored in a register.
        AddrOnStack, //!< Jump address is on the stack.
    };
    enum StackOpType {
        NoStackOp,     //!< Does not change the stack or SP.
        PushesOnStack, //!< PC is pushed onto the stack before jump.
        PopsFromStack, //!< PC is popped from the stack.
    };

    std::string name;
    uint8_t opcode;
    /// Number of CPU steps required to execute this instruction.
    size_t num_cpu_steps;
    AddrType addr_type;
    StackOpType stack_op_type;

    /**
     * @{
     * \name Randomly generated values
     */
    uint32_t mem_base;
    uint32_t init_pc;
    uint32_t jump_addr;
    std::optional<int8_t> pc_offset;
    std::optional<uint8_t> reg_code;

    bool flag_zero;
    bool flag_sign;
    bool flag_carry;
    bool flag_overflow;
    /// @}

    std::optional<std::function<bool(const FlowInstrParam &, cpu_ctx_t *)>>
        f_should_jump;

    static FlowInstrParam
    get_random_param(std::mt19937 &rng, std::string name, uint8_t opcode,
                     AddrType addr_type,
                     StackOpType stack_op_type = NoStackOp) {
        size_t num_cpu_steps = addr_type == AddrOnStack ? 2 : 3;
        FlowInstrParam param = {};
        param.name = name;
        param.opcode = opcode;
        param.num_cpu_steps = num_cpu_steps;
        param.addr_type = addr_type;
        param.stack_op_type = stack_op_type;
        param.set_random_vals(rng);
        return param;
    }

    void set_random_vals(std::mt19937 &rng) {
        mem_base = get_random_base_addr(rng);
        init_pc = mem_base;
        vm_addr_t prog_end = mem_base + 10; // 10 is enough to encode the instr
        vm_addr_t stack_bottom = mem_base + TEST_MEM_SIZE - 4;

        switch (addr_type) {
        case RelToPC: {
            uint32_t addr =
                get_random_data_addr(rng, prog_end, stack_bottom, 0);
            int32_t offset32 = addr - init_pc;
            pc_offset = (int8_t)offset32;
            jump_addr = init_pc + *pc_offset;
            break;
        }
        case AddrInIMM32:
        case AddrOnStack:
            jump_addr = get_random_data_addr(rng, prog_end, stack_bottom, 0);
            break;
        case AddrInReg: {
            jump_addr = get_random_data_addr(rng, prog_end, stack_bottom, 0);
            std::vector<uint8_t> used_reg_codes = {CPU_CODE_SP};
            *reg_code = get_random_reg_code(rng, true, used_reg_codes);
            break;
        }
        }

        flag_zero = get_random_bool(rng);
        flag_sign = get_random_bool(rng);
        flag_carry = get_random_bool(rng);
        flag_overflow = get_random_bool(rng);
    }

    std::vector<uint8_t> encode_instr() const {
        std::vector<uint8_t> bytes;
        bytes.push_back(opcode);
        switch (addr_type) {
        case AddrOnStack:
            break;
        case RelToPC:
            bytes.push_back((uint8_t)*pc_offset);
            break;
        case AddrInIMM32:
            bytes.resize(bytes.size() + 4);
            memcpy(bytes.data() + bytes.size() - 4, &jump_addr, 4);
            break;
        case AddrInReg:
            bytes.push_back(*reg_code);
            break;
        }
        return bytes;
    }

    void prepare_cpu(cpu_ctx_t *cpu) const {
        cpu->state = CPU_FETCH_DECODE_OPCODE;
        cpu->reg_pc = init_pc;
        cpu->reg_sp = mem_base + TEST_MEM_SIZE;

        if (addr_type == AddrInReg) {
            uint32_t *p_reg = get_reg_ptr(cpu, *reg_code);
            *p_reg = jump_addr;
        } else if (addr_type == AddrOnStack) {
            cpu->reg_sp -= 4;
            cpu->mem->write_u32(cpu->mem, cpu->reg_sp, jump_addr);
        }

        cpu->flags = 0;
        if (flag_zero) { cpu->flags |= CPU_FLAG_ZERO; }
        if (flag_sign) { cpu->flags |= CPU_FLAG_SIGN; }
        if (flag_carry) { cpu->flags |= CPU_FLAG_CARRY; }
        if (flag_overflow) { cpu->flags |= CPU_FLAG_OVERFLOW; }
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const FlowInstrParam &param) {
        std::string addr_desc = "<not impl>";
        switch (param.addr_type) {
        case FlowInstrParam::RelToPC:
            addr_desc = absl::StrFormat("%+d", *param.pc_offset);
            break;
        case FlowInstrParam::AddrInReg:
            addr_desc = absl::StrFormat("R[%u]", *param.reg_code);
            break;
        case FlowInstrParam::AddrInIMM32:
            addr_desc = "imm32";
            break;
        case FlowInstrParam::AddrOnStack:
            addr_desc = "[SP]";
            break;
        default:
            break;
        }
        std::string flag_str;
        if (param.flag_zero) {
            flag_str += "Z";
        } else {
            flag_str += "z";
        }
        if (param.flag_sign) {
            flag_str += "S";
        } else {
            flag_str += "s";
        }
        if (param.flag_carry) {
            flag_str += "C";
        } else {
            flag_str += "c";
        }
        if (param.flag_overflow) {
            flag_str += "O";
        } else {
            flag_str += "o";
        }
        os << absl::StrFormat("{ %s, PC = 0x%08X, %s = 0x%08X, %s }",
                              param.name, param.init_pc, addr_desc,
                              param.jump_addr, flag_str);
        return os;
    }
};

class FlowInstrTest : public testing::TestWithParam<FlowInstrParam> {
  protected:
    FlowInstrTest() {
        auto param = GetParam();
        auto instr_bytes = param.encode_instr();
        instr_size = instr_bytes.size();

        mem = new FakeMem(param.mem_base, param.mem_base + TEST_MEM_SIZE);
        mem->write(param.mem_base, instr_bytes.data(), instr_bytes.size());

        cpu = cpu_new(&mem->mem_if);
    }
    ~FlowInstrTest() {
        cpu_free(cpu);
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
    size_t instr_size;
};

TEST_P(FlowInstrTest, SetsPC) {
    auto param = GetParam();
    param.prepare_cpu(cpu);
    uint32_t orig_pc = cpu->reg_pc;

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);

    bool exp_jump = true;
    if (param.f_should_jump.has_value()) {
        exp_jump = param.f_should_jump.value()(param, cpu);
    }
    if (exp_jump) {
        EXPECT_EQ(cpu->reg_pc, param.jump_addr);
    } else {
        EXPECT_EQ(cpu->reg_pc, orig_pc + instr_size);
    }
}

TEST_P(FlowInstrTest, CheckStack) {
    auto param = GetParam();
    param.prepare_cpu(cpu);
    uint32_t orig_pc = cpu->reg_pc;
    uint32_t orig_sp = cpu->reg_sp;

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);

    int32_t sp_change = cpu->reg_sp - orig_sp;
    switch (param.stack_op_type) {
    case FlowInstrParam::NoStackOp:
        EXPECT_EQ(sp_change, 0);
        break;
    case FlowInstrParam::PushesOnStack: {
        ASSERT_EQ(sp_change, -4);
        uint32_t next_instr_at = orig_pc + instr_size;
        uint32_t val_on_stack = 0xDEADBEEF;
        vm_err_t err = mem->read(cpu->reg_sp, &val_on_stack, 4);
        ASSERT_EQ(err.type, VM_ERR_NONE);
        EXPECT_EQ(val_on_stack, next_instr_at);
        break;
    }
    case FlowInstrParam::PopsFromStack:
        ASSERT_EQ(sp_change, 4);
        break;
    }
}

INSTANTIATE_TEST_SUITE_P(Random_JMPR_V8, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JMPR_V8", CPU_OP_JMPR_V8,
                                     FlowInstrParam::RelToPC);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JMPA_V32, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JMPA_V32", CPU_OP_JMPA_V32,
                                     FlowInstrParam::AddrInIMM32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JMPA_R, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JMPA_R", CPU_OP_JMPA_R,
                                     FlowInstrParam::AddrInReg);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_JEQR_V8, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JEQR_V8", CPU_OP_JEQR_V8,
                                     FlowInstrParam::RelToPC);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JEQA_V32, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JEQA_V32", CPU_OP_JEQA_V32,
                                     FlowInstrParam::AddrInIMM32);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JEQA_R, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JEQA_R", CPU_OP_JEQA_R,
                                     FlowInstrParam::AddrInReg);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_JNER_V8, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JNER_V8", CPU_OP_JNER_V8,
                                     FlowInstrParam::RelToPC);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return !p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JNEA_V32, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JNEA_V32", CPU_OP_JNEA_V32,
                                     FlowInstrParam::AddrInIMM32);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return !p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JNEA_R, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JNEA_R", CPU_OP_JNEA_R,
                                     FlowInstrParam::AddrInReg);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return !p.flag_zero;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_JGTR_V8, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGTR_V8", CPU_OP_JGTR_V8, FlowInstrParam::RelToPC);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !p.flag_zero && !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JGTA_V32, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGTA_V32", CPU_OP_JGTA_V32, FlowInstrParam::AddrInIMM32);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !p.flag_zero && !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JGTA_R, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGTA_R", CPU_OP_JGTA_R, FlowInstrParam::AddrInReg);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !p.flag_zero && !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_JGER_V8, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGER_V8", CPU_OP_JGER_V8, FlowInstrParam::RelToPC);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JGEA_V32, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGEA_V32", CPU_OP_JGEA_V32, FlowInstrParam::AddrInIMM32);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JGEA_R, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JGEA_R", CPU_OP_JGEA_R, FlowInstrParam::AddrInReg);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return !(p.flag_sign ^ p.flag_overflow);
            };
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_JLTR_V8, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JLTR_V8", CPU_OP_JLTR_V8,
                                     FlowInstrParam::RelToPC);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_sign ^ p.flag_overflow;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JLTA_V32, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JLTA_V32", CPU_OP_JLTA_V32,
                                     FlowInstrParam::AddrInIMM32);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_sign ^ p.flag_overflow;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_JLTA_R, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "JLTA_R", CPU_OP_JLTA_R,
                                     FlowInstrParam::AddrInReg);
                                 param.f_should_jump =
                                     [](const FlowInstrParam &p, cpu_ctx_t *) {
                                         return p.flag_sign ^ p.flag_overflow;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_JLER_V8, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JLER_V8", CPU_OP_JLER_V8, FlowInstrParam::RelToPC);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return p.flag_zero || p.flag_sign ^ p.flag_overflow;
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JLEA_V32, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JLEA_V32", CPU_OP_JLEA_V32, FlowInstrParam::AddrInIMM32);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return p.flag_zero || p.flag_sign ^ p.flag_overflow;
            };
            v.push_back(param);
        }
        return v;
    }()));
INSTANTIATE_TEST_SUITE_P(
    Random_JLEA_R, FlowInstrTest, testing::ValuesIn([&] {
        std::vector<FlowInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = FlowInstrParam::get_random_param(
                rng, "JLEA_R", CPU_OP_JLEA_R, FlowInstrParam::AddrInReg);
            param.f_should_jump = [](const FlowInstrParam &p, cpu_ctx_t *) {
                return p.flag_zero || p.flag_sign ^ p.flag_overflow;
            };
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_CALLA_V32, FlowInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "CALLA_V32", CPU_OP_CALLA_V32,
                                     FlowInstrParam::AddrInIMM32,
                                     FlowInstrParam::PushesOnStack);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_CALLA_R, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "CALLA_R", CPU_OP_CALLA_R,
                                     FlowInstrParam::AddrInReg,
                                     FlowInstrParam::PushesOnStack);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_RET, FlowInstrTest, testing::ValuesIn([&] {
                             std::vector<FlowInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = FlowInstrParam::get_random_param(
                                     rng, "RET", CPU_OP_RET,
                                     FlowInstrParam::AddrOnStack,
                                     FlowInstrParam::PopsFromStack);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
