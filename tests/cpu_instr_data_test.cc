#include <absl/strings/str_format.h>
#include <gtest/gtest.h>
#include <random>

#include "cpu.h"
#include "fake_mem.h"

#define TEST_RNG_SEED         12345
#define TEST_NUM_RANDOM_CASES 50
#define TEST_MEM_SIZE         100

struct DataInstrParam {
    vm_addr_t mem_base;
    size_t cpu_exec_steps;
    std::vector<uint8_t> instr_bytes;

    uint32_t expected_value;
    std::optional<std::function<void(cpu_ctx_t *)>> f_prepare;
    std::function<uint32_t(cpu_ctx_t *)> f_get_actual_value;

    friend std::ostream &operator<<(std::ostream &os,
                                    const DataInstrParam &param) {
        os << absl::StrFormat("{ mem_base = 0x%08X, cpu_exec_steps = %u, "
                              "instr_bytes = <...>, expected_value = 0x%08X }",
                              param.mem_base, param.cpu_exec_steps,
                              param.expected_value);
        return os;
    }
};

class DataInstrTest : public testing::TestWithParam<DataInstrParam> {
  protected:
    DataInstrTest() {
        auto param = GetParam();

        mem = new FakeMem(param.mem_base, param.mem_base + TEST_MEM_SIZE);
        mem->write(param.mem_base, param.instr_bytes.data(),
                   param.instr_bytes.size());

        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = param.mem_base;
    }
    ~DataInstrTest() {
        delete cpu;
        delete mem;
    }

    void exec_instr() {
        auto param = GetParam();
        for (size_t step_idx = 0; step_idx < param.cpu_exec_steps; step_idx++) {
            cpu_step(cpu);
            ASSERT_NE(cpu->state, CPU_HANDLE_INT);
        }
        ASSERT_NE(cpu->state, CPU_HANDLE_INT);
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(DataInstrTest, WritesValue) {
    auto param = GetParam();
    if (param.f_prepare) { param.f_prepare.value()(cpu); }
    exec_instr();
    EXPECT_EQ(param.f_get_actual_value(cpu), param.expected_value);
}

static vm_addr_t get_random_addr(std::mt19937 &rng) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(0, VM_MAX_ADDR);
    return addr_dist(rng);
}

static uint8_t get_random_reg_code(std::mt19937 &rng) {
    std::array<uint8_t, 9> reg_codes = {
        CPU_CODE_R0, CPU_CODE_R1, CPU_CODE_R2, CPU_CODE_R3, CPU_CODE_R4,
        CPU_CODE_R5, CPU_CODE_R6, CPU_CODE_R7, CPU_CODE_SP,
    };
    static_assert(CPU_NUM_GP_REG_CODES == 8, "update test code");
    std::uniform_int_distribution<size_t> reg_dist(0, reg_codes.size() - 1);
    return reg_codes[reg_dist(rng)];
}

static uint8_t get_random_reg_codes(std::mt19937 &rng) {
    uint8_t reg_code1 = get_random_reg_code(rng);
    uint8_t reg_code2 = get_random_reg_code(rng);
    return (reg_code1 << 4) | reg_code2;
}

static uint32_t get_random_imm32(std::mt19937 &rng) {
    std::uniform_int_distribution<uint32_t> val_dist(0, UINT32_MAX);
    return val_dist(rng);
}

INSTANTIATE_TEST_SUITE_P(Random_MOV_VR, DataInstrTest, testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 vm_addr_t mem_base = get_random_addr(rng);
                                 uint8_t reg_code = get_random_reg_code(rng);
                                 uint32_t val = get_random_imm32(rng);

                                 DataInstrParam param;
                                 param.mem_base = mem_base;
                                 param.cpu_exec_steps = 4;

                                 param.instr_bytes.push_back(CPU_OP_MOV_VR);
                                 param.instr_bytes.push_back(reg_code);
                                 param.instr_bytes.resize(6);
                                 memcpy(param.instr_bytes.data() + 2, &val, 4);

                                 param.expected_value = val;
                                 param.f_get_actual_value =
                                     [reg_code](cpu_ctx_t *cpu) {
                                         uint32_t *p_reg;
                                         cpu_decode_reg(cpu, reg_code, &p_reg);
                                         return *p_reg;
                                     };
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(
    Random_MOV_RR, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            vm_addr_t mem_base = get_random_addr(rng);
            uint8_t reg_codes = get_random_reg_codes(rng);
            uint32_t val = get_random_imm32(rng);

            DataInstrParam param;
            param.mem_base = mem_base;
            param.cpu_exec_steps = 3;

            param.instr_bytes.push_back(CPU_OP_MOV_RR);
            param.instr_bytes.push_back(reg_codes);

            param.f_prepare = [reg_codes, val](cpu_ctx_t *cpu) {
                uint32_t *p_reg_src;
                cpu_decode_reg(cpu, (reg_codes >> 4) & 0x0F, &p_reg_src);
                *p_reg_src = val;
            };
            param.expected_value = val;
            param.f_get_actual_value = [reg_codes](cpu_ctx_t *cpu) {
                uint32_t *p_reg_dst;
                cpu_decode_reg(cpu, (reg_codes >> 0) & 0x0F, &p_reg_dst);
                return *p_reg_dst;
            };
            v.push_back(param);
        }
        return v;
    }()));
