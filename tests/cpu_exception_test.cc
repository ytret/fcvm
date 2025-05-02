#include <gtest/gtest.h>

#include "common.h"
#include "cpu.h"
#include "fake_mem.h"

#define TEST_BAD_MEM      0x1000'0000
#define TEST_BAD_OPCODE   0x00
#define TEST_BAD_REG_CODE 0x0F
#define TEST_BAD_IMM5     0xFF // does not fit into imm5

struct CPUExceptionParam {
    struct ExcDesc {
        vm_err_type_t exception;
        cpu_state_t on_state;

        /// In case `on_state` is `CPU_FETCH_DECODE_OPERANDS`, this member
        /// specifies which operand fails to fetch/decode.
        size_t opd_idx = 0;

        vm_addr_t isr_addr() const {
            return exception + 1;
        }
    };

    std::string name;
    std::vector<ExcDesc> exceptions;
    std::vector<uint8_t> prog_bytes;
    /// Number of good instructions to execute, not expecting exceptions.
    size_t num_ok_instr = 0;

    /// `true` if the IVT needs to be filled and mapped into memory.
    bool map_ivt = true;

    vm_addr_t mem_base = 0x0000'0000;
    size_t mem_size = CPU_IVT_ENTRY_SIZE * CPU_IVT_NUM_ENTRIES + 255;
    vm_addr_t prog_start = mem_base + CPU_IVT_ENTRY_SIZE * CPU_IVT_NUM_ENTRIES;
    vm_addr_t stack_top = mem_base + mem_size;

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

        mem = new FakeMem(param.mem_base, param.mem_base + param.mem_size);
        mem->write(param.prog_start, param.prog_bytes.data(),
                   param.prog_bytes.size());

        if (param.map_ivt) {
            std::vector<uint8_t> ivt_bytes = build_ivt();
            mem->write(CPU_IVT_ADDR, ivt_bytes.data(), ivt_bytes.size());
        }

        cpu = cpu_new(&mem->mem_if);
        cpu->state = CPU_FETCH_DECODE_OPCODE;
        cpu->reg_pc = param.prog_start;
        cpu->reg_sp = param.stack_top;
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
};

TEST_P(CPUExceptionTest, JumpsToISR) {
    auto param = GetParam();

    size_t num_ok = 0;
    while (num_ok < param.num_ok_instr) {
        do {
            cpu_step(cpu);
            ASSERT_EQ(cpu->num_nested_exc, 0);
        } while (cpu->state != CPU_EXECUTED_OK);
        num_ok++;
    }

    size_t exc_count = 0;
    while (exc_count < param.exceptions.size()) {
        uint32_t instr_pc = cpu->reg_pc;

        const auto &exc_desc = param.exceptions.at(exc_count);
        std::optional<CPUExceptionParam::ExcDesc> next_exc =
            (exc_count + 1) < param.exceptions.size()
                ? std::make_optional(param.exceptions.at(exc_count + 1))
                : std::nullopt;

        // Get to the step which causes an exception.
        while (true) {
            if (cpu->state == exc_desc.on_state) {
                if (exc_desc.on_state == CPU_FETCH_DECODE_OPERANDS) {
                    if (cpu->instr.next_operand == exc_desc.opd_idx) { break; }
                } else {
                    break;
                }
            }
            ASSERT_EQ(cpu->num_nested_exc, exc_count);
            cpu_step(cpu);
        }

        // Expect an exception.
        ASSERT_EQ(cpu->state, exc_desc.on_state);
        ASSERT_EQ(cpu->num_nested_exc, exc_count);
        cpu_step(cpu);
        exc_count++;
        ASSERT_EQ(cpu->num_nested_exc, exc_count);

        if (exc_count == 3) {
            ASSERT_EQ(cpu->state, CPU_TRIPLE_FAULT);
            break;
        }

        // Fetch an ISR.
        ASSERT_EQ(cpu->state, CPU_INT_FETCH_ISR_ADDR);
        if (next_exc && next_exc->on_state == CPU_INT_FETCH_ISR_ADDR) {
            continue;
        }
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, exc_count);
        EXPECT_EQ(cpu->curr_isr_addr, exc_desc.isr_addr());

        // Push bad PC on the stack.
        ASSERT_EQ(cpu->state, CPU_INT_PUSH_PC);
        if (next_exc && next_exc->on_state == CPU_INT_PUSH_PC) { continue; }
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, exc_count);
        uint32_t pushed_val = 0xDEADBEEF;
        mem->read(cpu->reg_sp, &pushed_val, 4);
        EXPECT_EQ(pushed_val, instr_pc);

        // Jump to the ISR.
        ASSERT_EQ(cpu->state, CPU_INT_JUMP);
        cpu_step(cpu);
        ASSERT_EQ(cpu->num_nested_exc, exc_count);
        EXPECT_EQ(cpu->reg_pc, exc_desc.isr_addr());
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllExceptions, CPUExceptionTest, testing::ValuesIn([&] {
        std::vector<CPUExceptionParam> v;

        v.push_back(CPUExceptionParam{
            .name = "BAD_MEM",
            .exceptions = {{VM_ERR_BAD_MEM, CPU_EXECUTE}},
            .prog_bytes = build_instr(CPU_OP_STR_RV0)
                              .reg_code(CPU_CODE_R0)
                              .imm32(TEST_BAD_MEM)
                              .bytes,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_OPCODE",
            .exceptions = {{VM_ERR_BAD_OPCODE, CPU_FETCH_DECODE_OPCODE}},
            .prog_bytes = build_instr(TEST_BAD_OPCODE).bytes,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_REG_CODE",
            .exceptions = {{VM_ERR_BAD_REG_CODE, CPU_FETCH_DECODE_OPERANDS, 0}},
            .prog_bytes =
                build_instr(CPU_OP_PUSH_R).reg_code(TEST_BAD_REG_CODE).bytes,
        });

        v.push_back(CPUExceptionParam{
            .name = "BAD_IMM5",
            .exceptions = {{VM_ERR_BAD_IMM5, CPU_FETCH_DECODE_OPERANDS, 1}},
            .prog_bytes = build_instr(CPU_OP_SHL_RV)
                              .reg_code(CPU_CODE_R0)
                              .imm5(TEST_BAD_IMM5)
                              .bytes,
        });

        v.push_back(CPUExceptionParam{
            .name = "TRIPLE_FAULT_STACK_OVERFLOW_PUSH",
            .exceptions =
                {
                    {VM_ERR_STACK_OVERFLOW, CPU_EXECUTE},
                    {VM_ERR_STACK_OVERFLOW, CPU_INT_PUSH_PC},
                    {VM_ERR_STACK_OVERFLOW, CPU_INT_PUSH_PC},
                },
            .prog_bytes =
                build_prog()
                    .instr(build_instr(CPU_OP_MOV_VR)
                               .reg_code(CPU_CODE_SP)
                               .imm32(0))
                    .instr(build_instr(CPU_OP_PUSH_R).reg_code(CPU_CODE_R0))
                    .bytes,
            .num_ok_instr = 1,
        });

        v.push_back(CPUExceptionParam{
            .name = "TRIPLE_FAULT_STACK_OVERFLOW_POP",
            .exceptions =
                {
                    {VM_ERR_STACK_OVERFLOW, CPU_EXECUTE},
                    {VM_ERR_BAD_MEM, CPU_INT_FETCH_ISR_ADDR},
                    {VM_ERR_BAD_MEM, CPU_INT_FETCH_ISR_ADDR},
                },
            .prog_bytes =
                build_prog()
                    .instr(build_instr(CPU_OP_POP_R).reg_code(CPU_CODE_R0))
                    .bytes,
            .num_ok_instr = 0,
            .map_ivt = false,
            .mem_base = 0xFFFF'FF00,
            .mem_size = 0x0000'00FF,
            .prog_start = 0xFFFF'FF00,
            .stack_top = 0xFFFF'FFFF,
        });

        return v;
    }()));
