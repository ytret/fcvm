#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_BAD_MEM      0x1000'0000
#define TEST_BAD_OPCODE   0x00
#define TEST_BAD_REG_CODE 0x0F
#define TEST_BAD_IMM5     0xFF // does not fit into imm5

struct CPUExceptionParam {
    std::string name;
    vm_err_type_t exception;
    std::vector<uint8_t> prog_bytes;

    /// Number of initial CPU steps that do not cause an exception.
    size_t num_init_steps;
    /// `true` if the instruction cannot be decoded.
    bool malformed_instr = false;
    /// `true` if the IVT needs to be filled and mapped into memory.
    bool map_ivt = true;

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

TEST_P(CPUExceptionTest, JumpsToISR) {
    auto param = GetParam();

    for (size_t step = 0; step < param.num_init_steps; step++) {
        ASSERT_EQ(cpu->num_nested_exc, 0);
        cpu_step(cpu);
    }

    if (!param.malformed_instr) {
        // Try to execute the instruction.
        ASSERT_EQ(cpu->state, CPU_EXECUTE);
        cpu_step(cpu);
    }

    // Fetch the ISR address.
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

INSTANTIATE_TEST_SUITE_P(
    AllExceptions, CPUExceptionTest, testing::ValuesIn([&] {
        std::vector<CPUExceptionParam> v;

        v.push_back(CPUExceptionParam{
            .name = "BAD_MEM",
            .exception = VM_ERR_BAD_MEM,
            .prog_bytes = build_instr(CPU_OP_STR_RV0)
                              .reg_code(CPU_CODE_R0)
                              .imm32(TEST_BAD_MEM)
                              .bytes,
            .num_init_steps = 3,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_OPCODE",
            .exception = VM_ERR_BAD_OPCODE,
            .prog_bytes = build_instr(TEST_BAD_OPCODE).bytes,
            .num_init_steps = 1,
            .malformed_instr = true,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_REG_CODE",
            .exception = VM_ERR_BAD_REG_CODE,
            .prog_bytes =
                build_instr(CPU_OP_PUSH_R).reg_code(TEST_BAD_REG_CODE).bytes,
            .num_init_steps = 2,
            .malformed_instr = true,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_IMM5",
            .exception = VM_ERR_BAD_IMM5,
            .prog_bytes = build_instr(CPU_OP_SHL_RV)
                              .reg_code(CPU_CODE_R0)
                              .imm5(TEST_BAD_IMM5)
                              .bytes,
            .num_init_steps = 3,
            .malformed_instr = true,
        });

        return v;
    }()));
