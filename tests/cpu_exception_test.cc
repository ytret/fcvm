#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_BAD_MEM 0x1000'0000

struct CPUExceptionParam {
    std::string name;
    bool map_ivt;
    vm_err_type_t exception;
    std::vector<uint8_t> prog_bytes;

    vm_addr_t isr_addr() const {
        return exception + 1;
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const CPUExceptionParam &param) {
        os << "{ " << param.name << " }";
        return os;
    }
};

class CPUExceptionTest : public testing::TestWithParam<CPUExceptionParam> {
  protected:
    CPUExceptionTest() {
        auto param = GetParam();

        size_t ivt_size = CPU_IVT_ENTRY_SIZE * CPU_IVT_NUM_ENTRIES;

        mem_base = param.map_ivt ? 0x0000'0000 : 0x0000'1000;
        mem_size = mem_base + ivt_size + 100;
        prog_start = mem_base + ivt_size;
        stack_top = mem_base + mem_size;

        mem = new FakeMem(mem_base, mem_size);
        mem->write(prog_start, param.prog_bytes.data(),
                   param.prog_bytes.size());

        if (param.map_ivt) {
            std::vector<uint8_t> ivt_bytes = build_ivt();
            mem->write(CPU_IVT_ADDR, ivt_bytes.data(), ivt_bytes.size());
        }

        cpu = cpu_new(&mem->mem_if);
        cpu->reg_pc = prog_start;
        cpu->reg_sp = stack_top;
    }
    ~CPUExceptionTest() {
        cpu_free(cpu);
        delete mem;
    }

    /// Builds an IVT where each entry is the interrupt number plus one.
    std::vector<uint8_t> build_ivt() {
        std::vector<vm_addr_t> ivt;
        static_assert(CPU_IVT_ENTRY_SIZE == sizeof(vm_addr_t));
        ivt.resize(CPU_IVT_NUM_ENTRIES);

        for (int idx = 0; idx < CPU_IVT_NUM_ENTRIES; idx++) {
            ivt[idx] = idx + 1;
        }

        std::vector<uint8_t> ivt_bytes;
        ivt_bytes.resize(sizeof(vm_addr_t) * ivt.size());
        memcpy(ivt_bytes.data(), ivt.data(), ivt_bytes.size());
        return ivt_bytes;
    }

    FakeMem *mem;
    cpu_ctx_t *cpu;

    vm_addr_t mem_base;
    size_t mem_size;
    vm_addr_t prog_start;
    vm_addr_t stack_top;
};

TEST_P(CPUExceptionTest, BadMem) {
    auto param = GetParam();

    for (int step = 0; step < 3; step++) {
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, 0);
    }

    // Try to write to bad memory.
    ASSERT_EQ(cpu->state, CPU_EXECUTE);
    cpu_step(cpu);

    // Fetch ISR address.
    ASSERT_EQ(cpu->num_nested_exc, 1);
    ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
    cpu_step(cpu);

    // Push PC.
    ASSERT_EQ(cpu->state, CPU_INT_PUSH_PC);
    cpu_step(cpu);
    uint32_t pushed_val;
    mem->read(stack_top - 4, &pushed_val, 4);
    EXPECT_EQ(pushed_val, prog_start);

    // Jump to the ISR.
    ASSERT_EQ(cpu->state, CPU_INT_JUMP);
    cpu_step(cpu);
    EXPECT_EQ(cpu->reg_pc, param.isr_addr());
    ASSERT_EQ(cpu->state, CPU_FETCH_DECODE_OPCODE);
}

INSTANTIATE_TEST_SUITE_P(AllExceptions, CPUExceptionTest,
                         testing::ValuesIn([&] {
                             std::vector<CPUExceptionParam> v;

                             v.push_back(CPUExceptionParam{
                                 .name = "BAD_MEM",
                                 .map_ivt = true,
                                 .exception = VM_ERR_BAD_MEM,
                                 .prog_bytes = build_instr(CPU_OP_STR_RV0)
                                                   .reg_code(CPU_CODE_R0)
                                                   .imm32(TEST_BAD_MEM)
                                                   .bytes,
                             });

                             return v;
                         }()));
