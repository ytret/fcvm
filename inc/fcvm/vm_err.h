/**
 * @file
 * Error definitions.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// A unified error type returned by various API functions.
typedef enum {
    /// No error; equivalent to `0` so it can be used in conditional statements.
    VM_ERR_NONE = 0,
    /// Bad memory access.
    VM_ERR_BAD_MEM,

    /// Unrecognized instruction opcode.
    VM_ERR_BAD_OPCODE,
    /// Unrecognized register code in an instruction.
    VM_ERR_BAD_REG_CODE,
    /// Bad `imm5` byte in an instruction (the value does not fit into 5 bits).
    VM_ERR_BAD_IMM5,

    /// An instruction performs a division by zero.
    VM_ERR_DIV_BY_ZERO,
    /// CPU state step will lead to a stack underflow or overflow.
    VM_ERR_STACK_OVERFLOW,

    /// Cannot raise an invalid IRQ line.
    VM_ERR_INVALID_IRQ_NUM,

    /// Bus controller has no free slot for new devices.
    VM_ERR_BUS_NO_FREE_SLOT,
    /// Bus controller cannot map a memory region of the requested size for the
    /// device.
    VM_ERR_BUS_NO_FREE_MEM,

    /// Memory controller cannot map a region because the region limit has been
    /// reached.
    VM_ERR_MEM_MAX_REGIONS,
    /// Memory controller cannot map a region because it overlaps with an
    /// already mapped region.
    VM_ERR_MEM_USED,
    /// Memory controller cannot resolve a memory access.
    VM_ERR_MEM_BAD_OP,
} vm_err_t;

#ifdef __cplusplus
}
#endif
