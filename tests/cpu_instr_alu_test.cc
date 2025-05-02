#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_MEM_BASE 0x1000
#define TEST_MEM_SIZE 100

struct ALUInstrParam {
    enum ResType {
        ResNotStored, //!< Operation result is not stored anywhere.
        /// Operation result is stored in the destination register.
        ResStoredInDstReg,
    };
    enum SrcType {
        NoSrc,      //!< This operation has no source operand.
        SrcInReg,   //!< Source operand is stored in a register.
        SrcInIMM5,  //!< Source operand is stored as an imm5.
        SrcInIMM32, //!< Source operand is stored as an imm32.
    };

    std::string name;
    uint8_t opcode;
    /// Number of CPU steps required to execute this instruction.
    size_t num_cpu_steps;
    ResType res_type;

    /**
     * @{
     * \name Operands description
     */
    uint8_t dst_reg_code; //!< Destination register.
    uint32_t dst_val;     //!< Original destination register value.
    /// Source register (if #src_type is #SrcType::RegSrc).
    std::optional<uint8_t> src_reg_code;
    /// Source value (if #src_type is not #SrcType::NoSrc).
    std::optional<uint32_t> src_val;
    /// Type of the source operand in this instruction.
    SrcType src_type;
    /// @}

    /**
     * @{
     * \name Expected results from the operation
     */
    uint32_t exp_res_val;
    bool exp_flag_zero;
    bool exp_flag_sign;
    bool exp_flag_carry;
    bool exp_flag_overflow;
    /// @}

    static ALUInstrParam get_random_param(std::mt19937 &rng, std::string name,
                                          uint8_t opcode, ResType res_type,
                                          SrcType src_type) {
        size_t num_cpu_steps = 3;
        if (src_type == SrcInIMM5 || src_type == SrcInIMM32) {
            num_cpu_steps++;
        }
        ALUInstrParam param = {};
        param.name = name;
        param.opcode = opcode;
        param.num_cpu_steps = num_cpu_steps;
        param.res_type = res_type;
        param.src_type = src_type;
        param.set_random_vals(rng);

        return param;
    }

    void set_random_vals(std::mt19937 &rng) {
        // Randomize registers.
        dst_reg_code = get_random_reg_code(rng);
        if (src_type == SrcInReg) { src_reg_code = get_random_reg_code(rng); }

        // Randomize original operands.
        dst_val = get_random_imm32(rng);
        src_val = get_random_imm32(rng);
        if (src_type == SrcInIMM5) { *src_val &= ~31; }
    }

    void set_exp_val_flags(uint32_t exp_val) {
        exp_res_val = exp_val;
        set_default_flags();
    }

    void set_default_flags() {
        // Some of the flags are always calculated the same way for every
        // instruction.
        exp_flag_zero = exp_res_val == 0;
        exp_flag_sign = (exp_res_val & (1 << 31)) != 0;
        // Set the default value of the other flags to false.
        exp_flag_carry = false;
        exp_flag_overflow = false;
    }

    std::vector<uint8_t> encode_instr() const {
        std::vector<uint8_t> v;
        v.push_back(opcode);

        // Dest reg and maybe src reg.
        if (src_type == SrcInReg) {
            uint8_t code_dst = dst_reg_code;
            uint8_t code_src = *src_reg_code;
            v.push_back((code_dst << 4) | code_src);
        } else {
            v.push_back(dst_reg_code);
        }

        // Src value.
        if (src_type == SrcInIMM5) {
            uint8_t imm5 = (*src_val) & 31;
            v.push_back(imm5);
        } else if (src_type == SrcInIMM32) {
            uint32_t imm32 = *src_val;
            v.resize(v.size() + 4);
            memcpy(v.data() + v.size() - 4, &imm32, 4);
        }

        return v;
    }

    void prepare_cpu(cpu_ctx_t *cpu) const {
        cpu->state = CPU_FETCH_DECODE_OPCODE;

        // First fill in the source register, so that if the src and dst regs
        // are the same, the caller can overwrite the expected value assuming
        // that the dst val is written to the reg.
        if (src_type == SrcInReg) {
            uint32_t *p_src_reg = get_reg_ptr(cpu, *src_reg_code);
            *p_src_reg = *src_val;
        }

        uint32_t *p_dst_reg = get_reg_ptr(cpu, dst_reg_code);
        *p_dst_reg = dst_val;
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const ALUInstrParam &param) {
        std::string src_desc = "no src";
        if (param.src_type == SrcInReg) {
            src_desc = absl::StrFormat("RS[%u] = 0x%08X", *param.src_reg_code,
                                       *param.src_val);
        } else if (param.src_type != NoSrc) {
            src_desc = absl::StrFormat("src = 0x%08X", *param.src_val);
        }
        os << absl::StrFormat("{ %s, RD[%u] = 0x%08X, %s, exp = 0x%08X }",
                              param.name, param.dst_reg_code, param.dst_val,
                              src_desc, param.exp_res_val);
        return os;
    }
};

class ALUInstrTest : public testing::TestWithParam<ALUInstrParam> {
  protected:
    ALUInstrTest() {
        auto param = GetParam();
        auto instr_bytes = param.encode_instr();

        mem = new FakeMem(TEST_MEM_BASE, TEST_MEM_BASE + TEST_MEM_SIZE);
        mem->write(TEST_MEM_BASE, instr_bytes.data(), instr_bytes.size());

        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = TEST_MEM_BASE;
    }
    ~ALUInstrTest() {
        cpu_free(cpu);
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(ALUInstrTest, WritesValue) {
    auto param = GetParam();
    param.prepare_cpu(cpu);

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }
    ASSERT_EQ(cpu->state, CPU_EXECUTED_OK);

    if (param.res_type == ALUInstrParam::ResStoredInDstReg) {
        uint32_t *p_reg_dst = get_reg_ptr(cpu, param.dst_reg_code);
        EXPECT_EQ(*p_reg_dst, param.exp_res_val);
    }
}

TEST_P(ALUInstrTest, WritesFlags) {
    auto param = GetParam();
    param.prepare_cpu(cpu);

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }
    ASSERT_EQ(cpu->state, CPU_EXECUTED_OK);

    bool act_flag_zero = (cpu->flags & CPU_FLAG_ZERO) != 0;
    bool act_flag_sign = (cpu->flags & CPU_FLAG_SIGN) != 0;
    bool act_flag_carry = (cpu->flags & CPU_FLAG_CARRY) != 0;
    bool act_flag_overflow = (cpu->flags & CPU_FLAG_OVERFLOW) != 0;
    EXPECT_EQ(act_flag_zero, param.exp_flag_zero);
    EXPECT_EQ(act_flag_sign, param.exp_flag_sign);
    EXPECT_EQ(act_flag_carry, param.exp_flag_carry);
    EXPECT_EQ(act_flag_overflow, param.exp_flag_overflow);
}

INSTANTIATE_TEST_SUITE_P(
    Random_ADD_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "ADD_RR", CPU_OP_ADD_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t op2;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val + param.dst_val);
                op2 = op1;
            } else {
                param.set_exp_val_flags(param.dst_val + *param.src_val);
                op2 = *param.src_val;
            }

            bool sign_op1 = (op1 & (1 << 31)) != 0;
            bool sign_op2 = (op2 & (1 << 31)) != 0;
            bool sign_res = (param.exp_res_val & (1 << 31)) != 0;
            param.exp_flag_carry = (((uint64_t)op1 + op2) >> 32) != 0;
            param.exp_flag_overflow =
                sign_op1 == sign_op2 && sign_res != sign_op1;

            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_ADD_RV, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "ADD_RV", CPU_OP_ADD_RV, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInIMM32);
            param.set_exp_val_flags(param.dst_val + *param.src_val);

            bool sign_dst = (param.dst_val & (1 << 31)) != 0;
            bool sign_src = (*param.src_val & (1 << 31)) != 0;
            bool sign_res = (param.exp_res_val & (1 << 31)) != 0;
            param.exp_flag_carry =
                (((uint64_t)param.dst_val + *param.src_val) >> 32) != 0;
            param.exp_flag_overflow =
                sign_dst == sign_src && sign_res != sign_dst;

            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SUB_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SUB_RR", CPU_OP_SUB_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t op2;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val - param.dst_val);
                op2 = op1;
            } else {
                param.set_exp_val_flags(param.dst_val - *param.src_val);
                op2 = *param.src_val;
            }

            bool sign_op1 = (op1 & (1 << 31)) != 0;
            bool sign_op2 = (op2 & (1 << 31)) != 0;
            bool sign_res = (param.exp_res_val & (1 << 31)) != 0;
            param.exp_flag_carry = op1 >= op2;
            param.exp_flag_overflow =
                sign_op1 != sign_op2 && sign_res != sign_op1;

            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SUB_RV, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SUB_RV", CPU_OP_SUB_RV, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInIMM32);
            param.set_exp_val_flags(param.dst_val - *param.src_val);

            bool sign_dst = (param.dst_val & (1 << 31)) != 0;
            bool sign_src = (*param.src_val & (1 << 31)) != 0;
            bool sign_res = (param.exp_res_val & (1 << 31)) != 0;
            param.exp_flag_carry = param.dst_val >= *param.src_val;
            param.exp_flag_overflow =
                sign_dst != sign_src && sign_res != sign_dst;

            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_MUL_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "MUL_RR", CPU_OP_MUL_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t op2;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val * param.dst_val);
                op2 = op1;
            } else {
                param.set_exp_val_flags(param.dst_val * *param.src_val);
                op2 = *param.src_val;
            }

            param.exp_flag_carry = (((uint64_t)op1 * op2) >> 32) != 0;
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_MUL_RV, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "MUL_RV", CPU_OP_MUL_RV, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInIMM32);
            param.set_exp_val_flags(param.dst_val * *param.src_val);

            param.exp_flag_carry =
                (((uint64_t)param.dst_val * *param.src_val) >> 32) != 0;
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_DIV_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "DIV_RR", CPU_OP_DIV_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val / param.dst_val);
            } else {
                param.set_exp_val_flags(param.dst_val / *param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_DIV_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "DIV_RV", CPU_OP_DIV_RV,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(param.dst_val /
                                                         *param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_IDIV_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "IDIV_RR", CPU_OP_IDIV_RR,
                ALUInstrParam::ResStoredInDstReg, ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags((int32_t)param.dst_val /
                                        (int32_t)param.dst_val);
            } else {
                param.set_exp_val_flags((int32_t)param.dst_val /
                                        (int32_t)*param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_IDIV_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "IDIV_RV", CPU_OP_IDIV_RV,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(
                                     (int32_t)param.dst_val /
                                     (int32_t)*param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_AND_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "AND_RR", CPU_OP_AND_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val & param.dst_val);
            } else {
                param.set_exp_val_flags(param.dst_val & *param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_AND_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "AND_RV", CPU_OP_AND_RV,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(param.dst_val &
                                                         *param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_OR_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "OR_RR", CPU_OP_OR_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val | param.dst_val);
            } else {
                param.set_exp_val_flags(param.dst_val | *param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_OR_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "OR_RV", CPU_OP_OR_RV,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(param.dst_val |
                                                         *param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_XOR_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "XOR_RR", CPU_OP_XOR_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val ^ param.dst_val);
            } else {
                param.set_exp_val_flags(param.dst_val ^ *param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_XOR_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "XOR_RV", CPU_OP_XOR_RV,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(param.dst_val ^
                                                         *param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_NOT_R, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "NOT_R", CPU_OP_NOT_R,
                                     ALUInstrParam::ResStoredInDstReg,
                                     ALUInstrParam::NoSrc);
                                 param.set_exp_val_flags(~param.dst_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SHL_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SHL_RR", CPU_OP_SHL_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t op2;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val << (param.dst_val & 31));
                op2 = op1;
            } else {
                param.set_exp_val_flags(param.dst_val << (*param.src_val & 31));
                op2 = *param.src_val;
            }
            param.exp_flag_carry =
                (((uint64_t)op1 >> (32 - (op2 & 31))) & 1) != 0;
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SHL_RV, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SHL_RV", CPU_OP_SHL_RV, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInIMM5);
            param.set_exp_val_flags(param.dst_val << (*param.src_val & 31));
            param.exp_flag_carry =
                (((uint64_t)param.dst_val >> (32 - (*param.src_val & 31))) &
                 1) != 0;
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SHR_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SHR_RR", CPU_OP_SHR_RR, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t num_bits;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val >> (param.dst_val & 31));
                num_bits = op1;
            } else {
                param.set_exp_val_flags(param.dst_val >> (*param.src_val & 31));
                num_bits = *param.src_val;
            }
            if (num_bits > 0) {
                param.exp_flag_carry = ((op1 >> (num_bits - 1)) & 1) != 0;
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_SHR_RV, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "SHR_RV", CPU_OP_SHR_RV, ALUInstrParam::ResStoredInDstReg,
                ALUInstrParam::SrcInIMM5);
            param.set_exp_val_flags(param.dst_val >> (*param.src_val & 31));
            param.exp_flag_carry =
                (((uint64_t)param.dst_val >> (32 - (*param.src_val & 31))) &
                 1) != 0;
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_CMP_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "CMP_RR", CPU_OP_CMP_RR, ALUInstrParam::ResNotStored,
                ALUInstrParam::SrcInReg);

            uint32_t op1 = param.dst_val;
            uint32_t op2;
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val - param.dst_val);
                op2 = op1;
            } else {
                param.set_exp_val_flags(param.dst_val - *param.src_val);
                op2 = *param.src_val;
            }

            bool sign_op1 = (op1 & (1 << 31)) != 0;
            bool sign_op2 = (op2 & (1 << 31)) != 0;
            bool sign_res = (param.exp_res_val & (1 << 31)) != 0;
            param.exp_flag_carry = op1 >= op2;
            param.exp_flag_overflow =
                sign_op1 != sign_op2 && sign_res != sign_op1;

            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_TST_RR, ALUInstrTest, testing::ValuesIn([&] {
        std::vector<ALUInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            auto param = ALUInstrParam::get_random_param(
                rng, "TST_RR", CPU_OP_TST_RR, ALUInstrParam::ResNotStored,
                ALUInstrParam::SrcInReg);
            if (param.dst_reg_code == *param.src_reg_code) {
                param.set_exp_val_flags(param.dst_val & param.dst_val);
            } else {
                param.set_exp_val_flags(param.dst_val & *param.src_val);
            }
            v.push_back(param);
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(Random_TST_RV, ALUInstrTest, testing::ValuesIn([&] {
                             std::vector<ALUInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = ALUInstrParam::get_random_param(
                                     rng, "TST_RV", CPU_OP_TST_RV,
                                     ALUInstrParam::ResNotStored,
                                     ALUInstrParam::SrcInIMM32);
                                 param.set_exp_val_flags(param.dst_val &
                                                         *param.src_val);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
