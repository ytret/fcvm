#include <gtest/gtest.h>
#include <random>

#include "cpu.h"
#include "fake_mem.h"

#define TEST_MEM_SIZE 100

using MOV_VR_Param = std::tuple<vm_addr_t, uint8_t, uint32_t>;

class MOV_VR_Test : public testing::TestWithParam<MOV_VR_Param> {
  protected:
    MOV_VR_Test() {
        auto [mem_base, reg_code, val] = GetParam();
        mem = new FakeMem(mem_base, mem_base + TEST_MEM_SIZE);
        cpu = cpu_new(&mem->mem_if);
    }
    ~MOV_VR_Test() {
        delete cpu;
        delete mem;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;
};

TEST_P(MOV_VR_Test, WritesToReg) {
    auto [mem_base, reg_code, val] = GetParam();

    uint8_t instr[6] = {CPU_OP_MOV_VR, reg_code};
    memcpy(&instr[2], &val, 4);
    mem->write(mem_base, instr, sizeof(instr));

    uint32_t *reg_ptr;
    cpu_decode_reg(cpu, reg_code, &reg_ptr);
    ASSERT_NE(reg_ptr, nullptr);

    cpu->reg_pc = mem_base;
    *reg_ptr = 0;

    for (int step_idx = 0; step_idx < 4; step_idx++) {
        cpu_step(cpu);
        ASSERT_NE(cpu->state, CPU_HANDLE_INT);
    }
    ASSERT_NE(cpu->state, CPU_HANDLE_INT);

    EXPECT_EQ(*reg_ptr, val);
}

INSTANTIATE_TEST_SUITE_P(
    RandomCases, MOV_VR_Test, testing::ValuesIn([&] {
        std::array<uint8_t, 9> reg_codes = {
            CPU_CODE_R0, CPU_CODE_R1, CPU_CODE_R2, CPU_CODE_R3, CPU_CODE_R4,
            CPU_CODE_R5, CPU_CODE_R6, CPU_CODE_R7, CPU_CODE_SP,
        };
        static_assert(CPU_NUM_GP_REG_CODES == 8, "update test code");
        std::uniform_int_distribution<size_t> reg_dist(0, reg_codes.size() - 1);

        std::uniform_int_distribution<vm_addr_t> base_dist(0, VM_MAX_ADDR);
        std::uniform_int_distribution<uint32_t> val_dist(0, UINT32_MAX);

        std::vector<MOV_VR_Param> v;
        std::mt19937 rng(12345);
        for (int i = 0; i < 50; i++) {
            vm_addr_t rnd_base = base_dist(rng);
            uint8_t reg = reg_codes[reg_dist(rng)];
            uint32_t val = val_dist(rng);
            v.emplace_back(rnd_base, reg, val);
        }
        return v;
    }()));
