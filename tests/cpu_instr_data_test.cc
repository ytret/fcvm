#include <gtest/gtest.h>
#include <random>

#include "cpu.h"
#include "fake_mem.h"

#define TEST_MEM_SIZE 100

using MOV_VR_Param = std::tuple<vm_addr_t, uint8_t, uint32_t>;
using MOV_RR_Param = std::tuple<vm_addr_t, uint8_t, uint32_t>;

class MOV_VR_Test : public testing::TestWithParam<MOV_VR_Param> {
  protected:
    MOV_VR_Test() {
        auto [mem_base, reg_code, val] = GetParam();
        mem = new FakeMem(mem_base, mem_base + TEST_MEM_SIZE);
        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = mem_base;

        cpu_decode_reg(cpu, reg_code, &reg_ptr);
        encode();
    }
    ~MOV_VR_Test() {
        delete cpu;
        delete mem;
    }

    void encode() {
        auto [mem_base, reg_code, val] = GetParam();
        uint8_t instr[6] = {CPU_OP_MOV_VR, reg_code};
        memcpy(&instr[2], &val, 4);
        mem->write(mem_base, instr, sizeof(instr));
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
    uint32_t *reg_ptr;
};

class MOV_RR_Test : public testing::TestWithParam<MOV_RR_Param> {
  protected:
    MOV_RR_Test() {
        auto [mem_base, reg_codes, val] = GetParam();
        mem = new FakeMem(mem_base, mem_base + TEST_MEM_SIZE);
        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = mem_base;

        cpu_decode_reg(cpu, (reg_codes >> 4) & 0x0F, &p_reg_src);
        cpu_decode_reg(cpu, (reg_codes >> 0) & 0x0F, &p_reg_dst);
        encode();
    }
    ~MOV_RR_Test() {
        delete cpu;
        delete mem;
    }

    void encode() {
        auto [mem_base, reg_codes, val] = GetParam();
        uint8_t instr[2] = {CPU_OP_MOV_RR, reg_codes};
        mem->write(mem_base, instr, 2);
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;

    uint32_t *p_reg_src;
    uint32_t *p_reg_dst;
};

TEST_P(MOV_VR_Test, WritesToReg) {
    auto [mem_base, reg_code, val] = GetParam();

    *reg_ptr = 0;

    for (int step_idx = 0; step_idx < 4; step_idx++) {
        cpu_step(cpu);
        ASSERT_NE(cpu->state, CPU_HANDLE_INT);
    }
    ASSERT_NE(cpu->state, CPU_HANDLE_INT);

    EXPECT_EQ(*reg_ptr, val);
}

TEST_P(MOV_RR_Test, WritesToReg) {
    auto [mem_base, reg_codes, val] = GetParam();

    cpu->reg_pc = mem_base;
    // dst must be written first in case dst and src are the same.
    *p_reg_dst = 0;
    *p_reg_src = val;

    for (int step_idx = 0; step_idx < 3; step_idx++) {
        cpu_step(cpu);
        ASSERT_NE(cpu->state, CPU_HANDLE_INT);
    }
    ASSERT_NE(cpu->state, CPU_HANDLE_INT);

    EXPECT_EQ(*p_reg_src, val);
    EXPECT_EQ(*p_reg_dst, val);
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

INSTANTIATE_TEST_SUITE_P(RandomCases, MOV_VR_Test, testing::ValuesIn([&] {
                             std::vector<MOV_VR_Param> v;
                             std::mt19937 rng(12345);
                             for (int i = 0; i < 50; i++) {
                                 vm_addr_t rnd_base = get_random_addr(rng);
                                 uint8_t reg_code = get_random_reg_code(rng);
                                 uint32_t val = get_random_imm32(rng);
                                 v.emplace_back(rnd_base, reg_code, val);
                             }
                             return v;
                         }()));
INSTANTIATE_TEST_SUITE_P(RandomCases, MOV_RR_Test, testing::ValuesIn([&] {
                             std::vector<MOV_RR_Param> v;
                             std::mt19937 rng(12345);
                             for (int i = 0; i < 50; i++) {
                                 vm_addr_t rnd_base = get_random_addr(rng);
                                 uint8_t reg_codes = get_random_reg_codes(rng);
                                 uint32_t val = get_random_imm32(rng);
                                 v.emplace_back(rnd_base, reg_codes, val);
                             }
                             return v;
                         }()));
