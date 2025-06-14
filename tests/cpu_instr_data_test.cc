#include <absl/strings/str_format.h>
#include <gtest/gtest.h>

#include "testcommon/fake_mem.h"
#include "testcommon/get_random.h"
#include <fcvm/cpu.h>

#define TEST_MEM_SIZE       100
#define TEST_MEM_START_DATA 50

struct DataInstrParam {
    enum DataDir {
        Imm32ToReg,
        RegToReg,
        RegToMem,
        MemToReg,
    };
    enum AddrType {
        NoAddr,
        AddrImm32,
        AddrRegNoOffset,
        AddrRegOffset8,
        AddrRegOffset32,
        AddrRegOffsetReg,
    };

    std::string name;
    uint8_t opcode;
    size_t num_cpu_steps;
    DataDir data_dir;
    AddrType addr_type;
    RegRef::Size reg_width;

    /// Operand values.
    struct {
        std::optional<RegRef> src_reg;
        std::optional<RegRef> dst_reg;
        std::optional<RegRef> off_reg;
        std::optional<vm_addr_t> addr_base;
        std::optional<int32_t> addr_offset;
    } operands;

    /// RAM map address.
    vm_addr_t mem_base;

    /// Value put into the register before the execution.
    uint32_t orig_val;
    /// Value being copied during the execution of the instruction.
    uint32_t exp_val;

    static DataInstrParam
    get_random_param(std::mt19937 &rng, std::string name, uint8_t opcode,
                     DataDir data_dir, AddrType addr_type,
                     RegRef::Size reg_width = RegRef::Size32Bits) {
        size_t num_cpu_steps = 4;
        switch (addr_type) {
        case AddrRegOffset8:
        case AddrRegOffset32:
        case AddrRegOffsetReg:
            num_cpu_steps += 1;
        case NoAddr:
        case AddrImm32:
        case AddrRegNoOffset:
            break;
        }

        DataInstrParam param = {};
        param.name = name;
        param.opcode = opcode;
        param.num_cpu_steps = num_cpu_steps;
        param.data_dir = data_dir;
        param.addr_type = addr_type;
        param.reg_width = reg_width;
        param.mem_base = get_random_base_addr(rng);
        param.set_random_operands(rng);
        param.exp_val = get_random_imm32(rng);
        return param;
    }

    void set_random_operands(std::mt19937 &rng) {
        bool has_src_reg = false;
        bool has_dst_reg = false;
        bool has_off_reg = false;
        bool has_addr = false;
        bool has_addr_offset = false;
        switch (data_dir) {
        case Imm32ToReg:
            has_dst_reg = true;
            break;
        case RegToReg:
            has_src_reg = true;
            has_dst_reg = true;
            break;
        case RegToMem:
            has_src_reg = true;
            has_dst_reg = addr_type != NoAddr && addr_type != AddrImm32;
            has_off_reg = addr_type == AddrRegOffsetReg;
            has_addr = true;
            has_addr_offset = addr_type == AddrRegOffset8 ||
                              addr_type == AddrRegOffset32 ||
                              addr_type == AddrRegOffsetReg;
            break;
        case MemToReg:
            has_src_reg = addr_type != NoAddr && addr_type != AddrImm32;
            has_dst_reg = true;
            has_off_reg = addr_type == AddrRegOffsetReg;
            has_addr = true;
            has_addr_offset = addr_type == AddrRegOffset8 ||
                              addr_type == AddrRegOffset32 ||
                              addr_type == AddrRegOffsetReg;
            break;
        }

        const bool unique_regs = data_dir == RegToMem || data_dir == MemToReg;
        std::vector<uint8_t> used_reg_codes;
        if (has_src_reg) {
            // If the source register holds a source memory address, the access
            // size is always 32 bits. If it holds a value, it's either 32 or 8
            // bits.
            operands.src_reg = RegRef{
                .size = data_dir == MemToReg ? RegRef::Size32Bits : reg_width,
                .code = get_random_reg_code(rng, unique_regs, used_reg_codes),
            };
            used_reg_codes.push_back(operands.src_reg->code);
        }
        if (has_dst_reg) {
            // Similarly to src_reg, if the destination register holds a memory
            // address, the access size is always 32 bits.
            operands.dst_reg = RegRef{
                .size = data_dir == RegToMem ? RegRef::Size32Bits : reg_width,
                .code = get_random_reg_code(rng, unique_regs, used_reg_codes),
            };
            used_reg_codes.push_back(operands.dst_reg->code);
        }
        if (has_off_reg) {
            operands.off_reg = RegRef{
                .size = RegRef::Size32Bits,
                .code = get_random_reg_code(rng, unique_regs, used_reg_codes),
            };
            used_reg_codes.push_back(operands.off_reg->code);
        }

        if (has_addr) {
            operands.addr_base =
                get_random_data_addr(rng, mem_base + TEST_MEM_START_DATA,
                                     mem_base + TEST_MEM_SIZE, 4);
            if (has_addr_offset) {
                const vm_addr_t target_addr =
                    get_random_data_addr(rng, mem_base + TEST_MEM_START_DATA,
                                         mem_base + TEST_MEM_SIZE, 4);
                operands.addr_offset = target_addr - *operands.addr_base;
            }
        }
    }

    std::vector<uint8_t> encode_instr() const {
        std::vector<uint8_t> v;
        v.push_back(opcode);

        switch (data_dir) {
        case Imm32ToReg:
            v.push_back(operands.dst_reg->encode());
            v.resize(v.size() + 4);
            memcpy(v.data() + v.size() - 4, &exp_val, 4);
            break;
        case RegToReg:
            v.push_back(operands.dst_reg->encode());
            v.push_back(operands.src_reg->encode());
            break;
        case RegToMem:
            encode_addr_operand(v);
            v.push_back(operands.src_reg->encode());
            break;
        case MemToReg:
            v.push_back(operands.dst_reg->encode());
            encode_addr_operand(v);
            break;
        }

        return v;
    }

    void encode_addr_operand(std::vector<uint8_t> &v) const {
        const vm_addr_t addr =
            operands.addr_base.value() + operands.addr_offset.value_or(0);
        const std::optional<RegRef> &reg =
            data_dir == RegToMem ? operands.dst_reg : operands.src_reg;
        switch (addr_type) {
        case NoAddr:
            break;
        case AddrImm32:
            v.resize(v.size() + 4);
            memcpy(v.data() + v.size() - 4, &addr, 4);
            break;
        case AddrRegNoOffset:
            v.push_back(reg->encode());
            break;
        case AddrRegOffset8:
            v.push_back(reg->encode());
            v.push_back(operands.addr_offset.value() & 0xFF);
            break;
        case AddrRegOffset32:
            v.push_back(reg->encode());
            v.resize(v.size() + 4);
            memcpy(v.data() + v.size() - 4, &operands.addr_offset.value(), 4);
            break;
        case AddrRegOffsetReg:
            v.push_back(reg->encode());
            v.push_back(operands.off_reg->encode());
            break;
        }
    }

    void prepare_cpu(cpu_ctx_t *cpu) const {
        cpu->state = CPU_FETCH_DECODE_OPCODE;

        // Depending on the data direction, the value to be copied is stored
        // either in the source register or at the memory address. If latter,
        // the address may be stored in the source or destination register.
        std::optional<RegRef> val_reg;
        std::optional<RegRef> addr_base_reg;

        if (data_dir == RegToReg || data_dir == RegToMem) {
            val_reg = operands.src_reg;
            if (data_dir == RegToMem && addr_type != AddrImm32) {
                addr_base_reg = operands.dst_reg;
            }
        } else if (data_dir == MemToReg && addr_type != AddrImm32) {
            addr_base_reg = operands.src_reg;
        }

        if (val_reg.has_value()) {
            if (reg_width == RegRef::Size32Bits) {
                *val_reg->get_ptr_u32(cpu) = exp_val;
            } else {
                *val_reg->get_ptr_u8(cpu) = exp_val & 0xFF;
            }
        }
        if (addr_base_reg.has_value()) {
            *addr_base_reg->get_ptr_u32(cpu) = *operands.addr_base;
        }
        if (addr_type == AddrRegOffsetReg) {
            *operands.off_reg->get_ptr_u32(cpu) = *operands.addr_offset;
        }

        if (data_dir == MemToReg) {
            vm_addr_t src_addr =
                operands.addr_base.value() + operands.addr_offset.value_or(0);
            if (reg_width == RegRef::Size32Bits) {
                cpu->mem->write_u32(cpu->mem, src_addr, exp_val);
            } else {
                cpu->mem->write_u8(cpu->mem, src_addr, exp_val & 0xFF);
            }
        }
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const DataInstrParam &param) {
        os << absl::StrFormat("{ %s }", param.name);
        return os;
    }
};

class DataInstrTest : public testing::TestWithParam<DataInstrParam> {
  protected:
    DataInstrTest() {
        auto param = GetParam();
        auto instr_bytes = param.encode_instr();

        mem = new FakeMem(param.mem_base, param.mem_base + TEST_MEM_SIZE);
        mem->write(param.mem_base, instr_bytes.data(), instr_bytes.size());

        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = param.mem_base;
    }
    ~DataInstrTest() {
        cpu_free(cpu);
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(DataInstrTest, WritesValue) {
    auto param = GetParam();
    param.prepare_cpu(cpu);

    for (size_t step_idx = 0; step_idx < param.num_cpu_steps; step_idx++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0) << "unexpected CPU exception";
    }
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE)
        << "invalid amount of CPU steps";

    switch (param.data_dir) {
    case DataInstrParam::Imm32ToReg:
    case DataInstrParam::RegToReg:
    case DataInstrParam::MemToReg:
        if (param.reg_width == RegRef::Size32Bits) {
            EXPECT_EQ(*param.operands.dst_reg->get_ptr_u32(cpu), param.exp_val);
        } else {
            // Only the lowest byte of the register is expected to be written
            // to. The upper three bytes are expected to be unchanged and equal
            // to zero.
            uint32_t exp_dword = param.exp_val & 0xFF;
            EXPECT_EQ(*param.operands.dst_reg->get_ptr_u32(cpu), exp_dword);
        }
        break;

    case DataInstrParam::RegToMem: {
        uint32_t act_val = 0;
        vm_addr_t dst_addr =
            *param.operands.addr_base + param.operands.addr_offset.value_or(0);
        cpu->mem->read_u32(cpu->mem, dst_addr, &act_val);

        if (param.reg_width == RegRef::Size32Bits) {
            EXPECT_EQ(act_val, param.exp_val);
        } else {
            // FakeMem is initialized with byte 0xFF.
            uint32_t exp_dword = 0xFFFFFF00;
            exp_dword |= param.exp_val & 0xFF;
            EXPECT_EQ(act_val, exp_dword);
        }
        break;
    }
    }
}

INSTANTIATE_TEST_SUITE_P(Random_MOV_VR, DataInstrTest, testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "MOV_VR", CPU_OP_MOV_VR,
                                     DataInstrParam::Imm32ToReg,
                                     DataInstrParam::NoAddr);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_MOV_RR, DataInstrTest, testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "MOV_RR", CPU_OP_MOV_RR,
                                     DataInstrParam::RegToReg,
                                     DataInstrParam::NoAddr);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_STR_RV0_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RV0 (32 bits)", CPU_OP_STR_RV0,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrImm32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI0_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI0 (32 bits)", CPU_OP_STR_RI0,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegNoOffset);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI8_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI8 (32 bits)", CPU_OP_STR_RI8,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffset8);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI32_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI32 (32 bits)", CPU_OP_STR_RI32,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffset32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RIR_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RIR (32 bits)", CPU_OP_STR_RIR,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffsetReg);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_STR_RV0_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RV0 (8 bits)", CPU_OP_STR_RV0,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrImm32,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI0_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI0 (8 bits)", CPU_OP_STR_RI0,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegNoOffset,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI8_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI8 (8 bits)", CPU_OP_STR_RI8,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffset8,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RI32_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RI32 (8 bits)", CPU_OP_STR_RI32,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffset32,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_STR_RIR_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "STR_RIR (8 bits)", CPU_OP_STR_RIR,
                                     DataInstrParam::RegToMem,
                                     DataInstrParam::AddrRegOffsetReg,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_LDR_RV0_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RV0 (32 bits)", CPU_OP_LDR_RV0,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrImm32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI0_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI0 (32 bits)", CPU_OP_LDR_RI0,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegNoOffset);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI8_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI8 (32 bits)", CPU_OP_LDR_RI8,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffset8);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI32_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI32 (32 bits)", CPU_OP_LDR_RI32,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffset32);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RIR_32, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RIR (32 bits)", CPU_OP_LDR_RIR,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffsetReg);
                                 v.push_back(param);
                             }
                             return v;
                         }()));

INSTANTIATE_TEST_SUITE_P(Random_LDR_RV0_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RV0 (8 bits)", CPU_OP_LDR_RV0,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrImm32,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI0_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI0 (8 bits)", CPU_OP_LDR_RI0,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegNoOffset,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI8_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI8 (8 bits)", CPU_OP_LDR_RI8,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffset8,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RI32_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RI32 (8 bits)", CPU_OP_LDR_RI32,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffset32,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(Random_LDR_RIR_8, DataInstrTest,
                         testing::ValuesIn([&] {
                             std::vector<DataInstrParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (int i = 0; i < TEST_NUM_RANDOM_CASES; i++) {
                                 auto param = DataInstrParam::get_random_param(
                                     rng, "LDR_RIR (8 bits)", CPU_OP_LDR_RIR,
                                     DataInstrParam::MemToReg,
                                     DataInstrParam::AddrRegOffsetReg,
                                     RegRef::Size8Bits);
                                 v.push_back(param);
                             }
                             return v;
                         }()));
