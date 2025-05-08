#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

#include "cpu.h"
#include "testcommon/fake_mem.h"
#include "testcommon/get_random.h"

#define TEST_MEM_SIZE   100
#define TEST_STACK_SIZE 4

struct StackInstrParam {
    enum InstrType {
        PushIMM32,
        PushReg,
        PopToReg,
    };

    std::string name;
    uint8_t opcode;
    /// Number of CPU steps required to execute this instruction.
    size_t num_cpu_steps;
    InstrType instr_type;

    /**
     * @{
     * \name Randomly generated values
     */
    uint32_t mem_base;
    uint32_t stack_top;
    uint32_t exp_val;
    std::optional<uint8_t> reg_code;
    /// @}

    static StackInstrParam get_random_param(std::mt19937 &rng, std::string name,
                                            uint8_t opcode,
                                            InstrType instr_type) {
        size_t num_cpu_steps = 3;
        StackInstrParam param = {};
        param.name = name;
        param.opcode = opcode;
        param.num_cpu_steps = num_cpu_steps;
        param.instr_type = instr_type;
        param.set_random_vals(rng);
        return param;
    }

    void set_random_vals(std::mt19937 &rng) {
        mem_base = get_random_base_addr(rng);
        vm_addr_t prog_end = mem_base + 10; // 10 is enough to encode the instr
        vm_addr_t stack_bottom = mem_base + TEST_MEM_SIZE - TEST_STACK_SIZE;
        stack_top =
            get_random_data_addr(rng, prog_end, stack_bottom, TEST_STACK_SIZE);
        exp_val = get_random_imm32(rng);
        if (instr_type == PushReg || instr_type == PopToReg) {
            reg_code = get_random_reg_code(rng, true, {CPU_CODE_SP});
        }
    }

    std::vector<uint8_t> encode_instr() const {
        std::vector<uint8_t> bytes;
        bytes.push_back(opcode);
        if (instr_type == PushIMM32) {
            bytes.resize(bytes.size() + 4);
            memcpy(bytes.data() + bytes.size() - 4, &exp_val, 4);
        } else {
            bytes.push_back(*reg_code);
        }
        return bytes;
    }

    void prepare_cpu(cpu_ctx_t *cpu) const {
        cpu->state = CPU_FETCH_DECODE_OPCODE;
        cpu->reg_pc = mem_base;
        cpu->reg_sp = stack_top;

        if (instr_type == PushReg) {
            uint32_t *p_reg = get_reg_ptr(cpu, *reg_code);
            *p_reg = exp_val;
        } else if (instr_type == PopToReg) {
            uint32_t *p_reg = get_reg_ptr(cpu, *reg_code);
            *p_reg = 0;
            cpu->reg_sp -= 4;
            cpu->mem->write_u32(cpu->mem, cpu->reg_sp, exp_val);
        }
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const StackInstrParam &param) {
        std::string instr_str;
        if (param.instr_type == PushIMM32 || param.instr_type == PushReg) {
            instr_str = "push";
        } else {
            instr_str = "pop";
        }
        std::string opd_str;
        if (param.instr_type == PushReg || param.instr_type == PopToReg) {
            opd_str = absl::StrFormat("R[%u]", *param.reg_code);
        } else {
            opd_str = "imm32";
        }
        os << absl::StrFormat("{ %s, %s %s 0x%08X, stack top 0x%08X }",
                              param.name, instr_str, opd_str, param.exp_val,
                              param.stack_top);
        return os;
    }
};

class StackInstrTest : public testing::TestWithParam<StackInstrParam> {
  protected:
    StackInstrTest() {
        auto param = GetParam();
        auto instr_bytes = param.encode_instr();

        mem = new FakeMem(param.mem_base, param.mem_base + TEST_MEM_SIZE);
        mem->write(param.mem_base, instr_bytes.data(), instr_bytes.size());

        cpu = cpu_new(&mem->mem_if);
        param.prepare_cpu(cpu);
    }
    ~StackInstrTest() {
        cpu_free(cpu);
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(StackInstrTest, SingleInstrWorks) {
    auto param = GetParam();
    uint32_t orig_sp = cpu->reg_sp;

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);

    int32_t sp_change = cpu->reg_sp - orig_sp;
    switch (param.instr_type) {
    case StackInstrParam::PushIMM32:
    case StackInstrParam::PushReg: {
        ASSERT_EQ(sp_change, -4);
        uint32_t pushed_val = 0xDEADBEEF;
        mem->read(cpu->reg_sp, &pushed_val, 4);
        EXPECT_EQ(pushed_val, param.exp_val);
        break;
    }

    case StackInstrParam::PopToReg:
        ASSERT_EQ(sp_change, 4);
        EXPECT_EQ(*get_reg_ptr(cpu, *param.reg_code), param.exp_val);
        break;
    }
}

INSTANTIATE_TEST_SUITE_P(Random_PUSH_V32, StackInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<StackInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = StackInstrParam::get_random_param(
                                     rng, "PUSH_V32", CPU_OP_PUSH_V32,
                                     StackInstrParam::PushIMM32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_PUSH_R, StackInstrTest, testing::ValuesIn([&] {
                             std::vector<StackInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = StackInstrParam::get_random_param(
                                     rng, "PUSH_R", CPU_OP_PUSH_R,
                                     StackInstrParam::PushReg);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_POP_R, StackInstrTest, testing::ValuesIn([&] {
                             std::vector<StackInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = StackInstrParam::get_random_param(
                                     rng, "POP_R", CPU_OP_POP_R,
                                     StackInstrParam::PopToReg);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
