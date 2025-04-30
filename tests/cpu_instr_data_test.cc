#include <absl/strings/str_format.h>
#include <gtest/gtest.h>
#include <iomanip>
#include <random>

#include "cpu.h"
#include "fake_mem.h"

#define TEST_RNG_SEED         12345
#define TEST_NUM_RANDOM_CASES 50

#define TEST_MEM_SIZE       100
#define TEST_MEM_START_DATA 50

struct DataInstrParam {
    vm_addr_t mem_base;
    size_t cpu_exec_steps;
    std::vector<uint8_t> instr_bytes;

    uint32_t expected_value;
    std::optional<uint32_t> mem_addr;
    std::optional<int32_t> mem_offset;

    std::optional<std::function<void(const DataInstrParam &, cpu_ctx_t *)>>
        f_prepare;
    std::function<uint32_t(const DataInstrParam &, cpu_ctx_t *)>
        f_get_actual_value;

    friend std::ostream &operator<<(std::ostream &os,
                                    const DataInstrParam &param) {
        std::string instr_hex = std::accumulate(
            param.instr_bytes.cbegin(), param.instr_bytes.cend(), std::string{},
            [](std::string acc, uint8_t b) {
                std::ostringstream oss;
                oss << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << static_cast<int>(b);
                return acc.empty() ? oss.str()
                                   : std::move(acc) + ' ' + oss.str();
            });
        std::string mem_addr = param.mem_addr.has_value()
                                   ? absl::StrFormat("0x%08X", *param.mem_addr)
                                   : std::string("<none>");
        std::string mem_offset =
            param.mem_addr.has_value()
                ? absl::StrFormat("%d", *param.mem_offset)
                : std::string("<none>");
        os << absl::StrFormat(
            "{ mem_base = 0x%08X, instr_bytes = [%s], expected_value = 0x%08X, "
            "mem_addr = %s, mem_offset = %s }",
            param.mem_base, instr_hex, param.expected_value, mem_addr,
            mem_offset);
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

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(DataInstrTest, WritesValue) {
    auto param = GetParam();
    if (param.f_prepare) { param.f_prepare.value()(param, cpu); }

    for (size_t step_idx = 0; step_idx < param.cpu_exec_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_NE(cpu->state, CPU_HANDLE_INT);
    }
    ASSERT_EQ(cpu->state, CPU_EXECUTED_OK);

    EXPECT_EQ(param.f_get_actual_value(param, cpu), param.expected_value);
}

static vm_addr_t get_random_base_addr(std::mt19937 &rng) {
    std::uniform_int_distribution<vm_addr_t> addr_dist(0, VM_MAX_ADDR);
    return addr_dist(rng);
}

static vm_addr_t get_random_data_addr(std::mt19937 &rng, vm_addr_t mem_base,
                                      size_t min_bytes_left) {
    vm_addr_t data_start = mem_base + TEST_MEM_START_DATA;
    vm_addr_t data_end = mem_base + TEST_MEM_SIZE;
    std::uniform_int_distribution<vm_addr_t> addr_dist(
        data_start, data_end - min_bytes_left);
    return addr_dist(rng);
}

static uint8_t get_random_reg_code(std::mt19937 &rng, bool unique_regs = false,
                                   std::vector<uint8_t> used_reg_codes = {}) {
    std::array<uint8_t, 9> reg_codes = {
        CPU_CODE_R0, CPU_CODE_R1, CPU_CODE_R2, CPU_CODE_R3, CPU_CODE_R4,
        CPU_CODE_R5, CPU_CODE_R6, CPU_CODE_R7, CPU_CODE_SP,
    };
    static_assert(CPU_NUM_GP_REG_CODES == 8, "update test code");
    std::uniform_int_distribution<size_t> reg_dist(0, reg_codes.size() - 1);
    uint8_t reg_code;
    do {
        reg_code = reg_codes[reg_dist(rng)];
    } while (unique_regs &&
             std::find(used_reg_codes.cbegin(), used_reg_codes.cend(),
                       reg_code) != used_reg_codes.cend());
    return reg_code;
}

static uint8_t get_random_reg_codes(std::mt19937 &rng, bool unique_regs = {},
                                    std::vector<uint8_t> used_reg_codes = {}) {
    uint8_t reg_code1 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    used_reg_codes.push_back(reg_code1);
    uint8_t reg_code2 = get_random_reg_code(rng, unique_regs, used_reg_codes);
    return (reg_code1 << 4) | reg_code2;
}

static uint32_t get_random_imm32(std::mt19937 &rng) {
    std::uniform_int_distribution<uint32_t> val_dist(0, UINT32_MAX);
    return val_dist(rng);
}

enum class ImmOperandRole {
    None,
    ExpVal,
    MemAddr,
    Offset,
};

/**
 * Creates a #DataInstrParam with randomly generated values.
 * @param rng -- Random number generator.
 * @param opcode -- Opcode of the instruction under test.
 * @param unique_regs -- Whether or not registers must be unique.
 * @param gen_mem_addr -- Generate #DataInstrParam.mem_addr.
 * @param gen_mem_offset -- Generate #DataInstrParam.mem_offset.
 * @param imm_opd_role -- Role of the immediate operand in the instruction.
 * @param f_prepare -- An optional function which will be used to prepare
 *   the execution context (e.g. put the expected value into memory).
 * @param f_get_act_val -- A required function to get the actual value in the
 *   test environment after executing the instruction.
 */
static DataInstrParam get_random_param(
    std::mt19937 &rng, uint8_t opcode, bool unique_regs, bool gen_mem_addr,
    bool gen_mem_offset, ImmOperandRole imm_opd_role,
    std::optional<std::function<void(const DataInstrParam &, cpu_ctx_t *)>>
        f_prepare,
    std::function<uint32_t(const DataInstrParam &, cpu_ctx_t *)>
        f_get_act_val) {
    const cpu_instr_desc_t *desc = cpu_lookup_instr_desc(opcode);
    if (desc == nullptr) {
        fprintf(stderr, "failed to lookup a descritptor for opcode 0x%02X\n",
                opcode);
        abort();
    }

    uint32_t exp_val = get_random_imm32(rng);
    uint32_t mem_base = get_random_base_addr(rng);

    std::optional<uint32_t> mem_addr;
    if (gen_mem_addr) { mem_addr = get_random_data_addr(rng, mem_base, 4); }

    std::optional<int32_t> mem_offset;
    if (gen_mem_addr && gen_mem_offset) {
        // offseted_addr is just used to get an offset within the RAM. Since the
        // offset may be truncated depending on the opcode, it may not
        // adequately represent the actual memory address the instruction is
        // supposed to work with.
        //
        // Truncated version is saved in mem_offset and can be used, since it's
        // what is encoded and together with mem_addr points to a valid RAM
        // dword.
        uint32_t offseted_addr = get_random_data_addr(rng, mem_base, 4);
        int32_t offset = offseted_addr - *mem_addr;
        mem_offset = static_cast<int32_t>(offset);
    }

    std::vector<uint8_t> instr_bytes = {opcode};
    std::vector<uint8_t> used_reg_codes;
    for (size_t idx = 0; idx < desc->num_operands; idx++) {
        cpu_operand_type_t opd = desc->operands[idx];
        switch (opd) {
        case CPU_OPD_REG: {
            uint8_t reg_code =
                get_random_reg_code(rng, unique_regs, used_reg_codes);
            used_reg_codes.push_back(reg_code);
            instr_bytes.push_back(reg_code);
            break;
        }
        case CPU_OPD_REGS: {
            uint8_t reg_codes =
                get_random_reg_codes(rng, unique_regs, used_reg_codes);
            used_reg_codes.push_back((reg_codes >> 4) & 0x0F);
            used_reg_codes.push_back((reg_codes >> 0) & 0x0F);
            instr_bytes.push_back(reg_codes);
            break;
        }
        case CPU_OPD_IMM5:
            // not used for data instructions
            break;
        case CPU_OPD_IMM8: {
            uint8_t imm8 = 0xAA;
            switch (imm_opd_role) {
            case ImmOperandRole::Offset:
                *mem_offset = static_cast<int8_t>(*mem_offset);
                imm8 = *mem_offset;
                break;
            default:
                fprintf(stderr, "bad test configuration: imm8 may be used only "
                                "as an offset\n");
            }
            instr_bytes.push_back(imm8);
            break;
        }
        case CPU_OPD_IMM32:
            uint32_t imm32 = 0xCAFEBABE;
            switch (imm_opd_role) {
            case ImmOperandRole::ExpVal:
                imm32 = exp_val;
                break;
            case ImmOperandRole::MemAddr:
                imm32 = *mem_addr;
                break;
            case ImmOperandRole::Offset:
                imm32 = *mem_offset;
                break;
            default:
                fprintf(stderr, "bad test configuration: imm_opd_role = "
                                "Missing, but opcode requires an imm value");
                abort();
            }
            instr_bytes.resize(instr_bytes.size() + 4);
            memcpy(instr_bytes.data() + instr_bytes.size() - 4, &imm32, 4);
            break;
        }
    }

    return DataInstrParam{
        .mem_base = mem_base,
        .cpu_exec_steps = 2 + desc->num_operands,
        .instr_bytes = instr_bytes,
        .expected_value = exp_val,
        .mem_addr = mem_addr,
        .mem_offset = mem_offset,
        .f_prepare = f_prepare,
        .f_get_actual_value = f_get_act_val,
    };
}

uint32_t *get_reg_ptr(cpu_ctx_t *cpu, uint8_t reg_code) {
    uint32_t *p_reg;
    cpu_decode_reg(cpu, reg_code, &p_reg);
    if (!p_reg) {
        fprintf(stderr, "failed to decode register code 0x%02X\n", reg_code);
        abort();
    }
    return p_reg;
}

INSTANTIATE_TEST_SUITE_P(
    Random_MOV_VR, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_MOV_VR, false, false, false, ImmOperandRole::ExpVal,
                {}, [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_code = param.instr_bytes.at(1);
                    return *get_reg_ptr(cpu, reg_code);
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_MOV_RR, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_MOV_RR, false, false, false, ImmOperandRole::None,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    *get_reg_ptr(cpu, (param.instr_bytes.at(1) >> 4) & 0x0F) =
                        param.expected_value;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    return *get_reg_ptr(cpu,
                                        (param.instr_bytes.at(1) >> 0) & 0x0F);
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_STR_RV0, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_STR_RV0, false, true, false,
                ImmOperandRole::MemAddr,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    *get_reg_ptr(cpu, param.instr_bytes.at(1)) =
                        param.expected_value;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint32_t act_val = 0xDEADBEEF;
                    cpu->mem->read_u32(cpu->mem, *param.mem_addr, &act_val);
                    return act_val;
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_STR_RI0, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_STR_RI0, true, true, false, ImmOperandRole::None,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_src = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    uint8_t reg_dst_mem = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    *get_reg_ptr(cpu, reg_src) = param.expected_value;
                    *get_reg_ptr(cpu, reg_dst_mem) = *param.mem_addr;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint32_t act_val = 0xDEADBEEF;
                    cpu->mem->read_u32(cpu->mem, *param.mem_addr, &act_val);
                    return act_val;
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_STR_RI8, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_STR_RI8, true, true, true, ImmOperandRole::Offset,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_src = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    uint8_t reg_dst_mem = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    *get_reg_ptr(cpu, reg_src) = param.expected_value;
                    *get_reg_ptr(cpu, reg_dst_mem) = *param.mem_addr;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint32_t act_val = 0xDEADBEEF;
                    vm_addr_t mem_addr = *param.mem_addr + *param.mem_offset;
                    cpu->mem->read_u32(cpu->mem, mem_addr, &act_val);
                    return act_val;
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_STR_RI32, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_STR_RI32, true, true, true, ImmOperandRole::Offset,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_src = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    uint8_t reg_dst_mem = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    *get_reg_ptr(cpu, reg_src) = param.expected_value;
                    *get_reg_ptr(cpu, reg_dst_mem) = *param.mem_addr;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint32_t act_val = 0xDEADBEEF;
                    vm_addr_t mem_addr = *param.mem_addr + *param.mem_offset;
                    cpu->mem->read_u32(cpu->mem, mem_addr, &act_val);
                    return act_val;
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_STR_RIR, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_STR_RIR, true, true, true, ImmOperandRole::None,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_src = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    uint8_t reg_dst_mem = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    uint8_t reg_dst_off = param.instr_bytes.at(2) & 0x0F;
                    *get_reg_ptr(cpu, reg_src) = param.expected_value;
                    *get_reg_ptr(cpu, reg_dst_mem) = *param.mem_addr;
                    *get_reg_ptr(cpu, reg_dst_off) = *param.mem_offset;
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint32_t act_val = 0xDEADBEEF;
                    vm_addr_t mem_addr = *param.mem_addr + *param.mem_offset;
                    cpu->mem->read_u32(cpu->mem, mem_addr, &act_val);
                    return act_val;
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_LDR_RV0, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_LDR_RV0, false, true, false,
                ImmOperandRole::MemAddr,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    cpu->mem->write_u32(cpu->mem, *param.mem_addr,
                                        param.expected_value);
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_dst = param.instr_bytes.at(1);
                    return *get_reg_ptr(cpu, reg_dst);
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_LDR_RI0, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_LDR_RI0, true, true, false, ImmOperandRole::None,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_mem = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    *get_reg_ptr(cpu, reg_mem) = *param.mem_addr;
                    cpu->mem->write_u32(cpu->mem, *param.mem_addr,
                                        param.expected_value);
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_dst = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    return *get_reg_ptr(cpu, reg_dst);
                }));
        }
        return v;
    }()));

INSTANTIATE_TEST_SUITE_P(
    Random_LDR_RI8, DataInstrTest, testing::ValuesIn([&] {
        std::vector<DataInstrParam> v;
        std::mt19937 rng(TEST_RNG_SEED);
        for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
            v.push_back(get_random_param(
                rng, CPU_OP_LDR_RI8, true, true, true, ImmOperandRole::Offset,
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_mem = (param.instr_bytes.at(1) >> 4) & 0x0F;
                    *get_reg_ptr(cpu, reg_mem) = *param.mem_addr;
                    cpu->mem->write_u32(cpu->mem,
                                        *param.mem_addr + *param.mem_offset,
                                        param.expected_value);
                },
                [](const DataInstrParam &param, cpu_ctx_t *cpu) {
                    uint8_t reg_dst = (param.instr_bytes.at(1) >> 0) & 0x0F;
                    return *get_reg_ptr(cpu, reg_dst);
                }));
        }
        return v;
    }()));
