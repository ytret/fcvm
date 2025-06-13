/**
 * @file cpu.h
 * CPU core API: instruction fetching, decoding and execution.
 */

#pragma once

#include <stdint.h>

#include <fcvm/cpu_instr.h>
#include <fcvm/cpu_instr_descs.h>
#include <fcvm/intctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Version of the `cpu_ctx_t` structure and its member structures.
/// Increment this every time anything in the `cpu_ctx_t` structure or its
/// member structures is changed: field order, size, type, etc.
#define SN_CPU_CTX_VER ((uint32_t)1)

#define CPU_NUM_GP_REGS 8
static_assert(CPU_NUM_GP_REGS == CPU_NUM_GP_REG_CODES,
              "update register codes in cpu_instr_descs.h");

#define CPU_FLAG_ZERO     (1 << 0)
#define CPU_FLAG_SIGN     (1 << 1)
#define CPU_FLAG_CARRY    (1 << 2)
#define CPU_FLAG_OVERFLOW (1 << 3)

#define CPU_IVT_ADDR        ((vm_addr_t)0x00000000)
#define CPU_IVT_ENTRY_SIZE  sizeof(vm_addr_t)
#define CPU_IVT_NUM_ENTRIES 256
#define CPU_IVT_SIZE        (CPU_IVT_ENTRY_SIZE * CPU_IVT_NUM_ENTRIES)
#define CPU_IVT_ENTRY_ADDR(entry_idx)                                          \
    (CPU_IVT_ADDR + CPU_IVT_ENTRY_SIZE * (entry_idx))
#define CPU_IVT_FIRST_IRQ_ENTRY 32

typedef enum {
    CPU_RESET,
    CPU_FETCH_DECODE_OPCODE,
    CPU_FETCH_DECODE_OPERANDS,
    CPU_EXECUTE,

    CPU_HALTED,
    CPU_INT_FETCH_ISR_ADDR,
    CPU_INT_PUSH_PC,
    CPU_INT_JUMP,
    CPU_TRIPLE_FAULT,
} cpu_state_t;

/// Exception numbers, with values corresponding to IVT entries.
/// These must fit into `uint8_t`.
typedef enum {
    CPU_EXC_RESET,
    CPU_EXC_BAD_MEM,
    CPU_EXC_BAD_INSTR,
    CPU_EXC_DIV_BY_ZERO,
    CPU_EXC_STACK_OVERFLOW,

    CPU_NUM_EXCEPTIONS,
} cpu_exc_type_t;
static_assert(CPU_NUM_EXCEPTIONS <= 256,
              "cpu_exc_type does not fit into uint8_t");
static_assert(CPU_NUM_EXCEPTIONS < CPU_IVT_FIRST_IRQ_ENTRY,
              "IVT has no space for this many exceptions, increase "
              "CPU_IVT_FIRST_IRQ_ENTRY");

typedef struct cpu_ctx {
    cpu_state_t state;
    cpu_instr_t instr;

    uint32_t gp_regs[CPU_NUM_GP_REGS];
    uint32_t reg_pc;
    uint32_t reg_sp;
    uint8_t flags;
    uint64_t cycles;

    mem_if_t *mem;

    /**
     * An interrupt controller responsible for CPU interrupts.
     * Passed to the @ref busctl.c "bus controller" which assigns IRQ lines for
     * connected devices.
     */
    intctl_ctx_t *intctl;

    size_t num_nested_exc;
    uint8_t curr_int_line;
    vm_addr_t curr_isr_addr;
    uint32_t pc_after_isr;
} cpu_ctx_t;

cpu_ctx_t *cpu_new(mem_if_t *mem);
void cpu_free(cpu_ctx_t *cpu);

/// @addtogroup snapshots
/// @{

/// Calculates the size of a buffer required to store a #cpu_ctx_t snapshot.
size_t cpu_snapshot_size(void);
/**
 * Writes a snapshot of @a cpu into the buffer @a v_buf.
 * @param cpu      CPU core to save a snapshot of.
 * @param v_buf    Snapshot buffer.
 * @param max_size Size of @a v_buf.
 * @returns Size of the saved snapshot in bytes.
 * @note @a max_size should be more than or equal to the size returned by
 * #cpu_snapshot_size().
 */
size_t cpu_snapshot(const cpu_ctx_t *cpu, void *v_buf, size_t max_size);
/**
 * Restores a #cpu_ctx_t structure from a snapshot buffer.
 * @param      mem           Memory controller for the new CPU context.
 * @param      v_buf         Snapshot buffer.
 * @param      max_size      Size of @a v_buf.
 * @param[out] out_used_size Number of bytes used from the buffer @a v_buf.
 * @returns A newly created CPU context restored from the buffer @a v_buf, which
 * uses @a mem as its memory controller.
 */
cpu_ctx_t *cpu_restore(mem_if_t *mem, const void *v_buf, size_t max_size,
                       size_t *out_used_size);
/// @}

void cpu_step(cpu_ctx_t *cpu);

vm_err_t cpu_decode_reg(cpu_ctx_t *cpu, uint8_t reg_ref,
                        cpu_reg_ref_t *out_reg_ref);
cpu_exc_type_t cpu_exc_type_of_err(cpu_ctx_t *cpu, vm_err_t err);

vm_err_t cpu_raise_irq(cpu_ctx_t *cpu, uint8_t irq_line);

#ifdef __cplusplus
}
#endif
