#pragma once

#include <fcvm/busctl.h>
#include <fcvm/cpu.h>
#include <fcvm/memctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Version of the `vm_ctx_t` structure and its member structures.
/// Increment this every time anything in the `vm_ctx_t` structure or its member
/// structures is changed: field order, size, type, etc.
#define SN_VM_CTX_VER ((uint32_t)1)

typedef struct {
    memctl_ctx_t *memctl;
    cpu_ctx_t *cpu;
    busctl_ctx_t *busctl;
} vm_ctx_t;

vm_ctx_t *vm_new(void);
void vm_free(vm_ctx_t *vm);

/**
 * @defgroup snapshots State snapshots
 * @brief Snapshots of the VM state
 *
 * These functions allow saving the current VM state into a snapshot (a byte
 * sequence) and restoring a VM state from a snapshot even in a different
 * process.
 *
 * The process of creating and restoring a snapshot is as follows:
 *
 * 1. Allocate a buffer of the size returned by #vm_snapshot_size().
 * 2. Write a snapshot of the VM state into the buffer by calling
 *    #vm_snapshot().
 * 3. The buffer now contains the serialized VM state and can be saved to a file
 *    or sent to another process.
 * 4. Restore the VM state from the buffer by calling #vm_restore(). This can be
 *    done in a different process.
 *
 * The snapshot buffer is structured like an onion, with each layer representing
 * a separate context (e.g., CPU or memory controller). Each corresponding
 * module provides its own size, snapshot, and restore functions.
 *
 * @{
 */
/// Calculates the size of a buffer required to store a snapshot of @a vm.
/// @param vm VM context.
/// @returns Minimum size of a buffer in bytes that is enough to fit the current
/// state of @a vm.
size_t vm_snapshot_size(const vm_ctx_t *vm);
/// Writes a snapshot of @a vm into the buffer @a v_buf.
/// @param vm VM context to save a snapshot of.
/// @param v_buf Snapshot buffer.
/// @param max_size Size of @a v_buf.
/// @returns Size of the saved snapshot in bytes.
/// @note @a max_size should be more than or equal to the size returned by
/// #vm_snapshot_size().
size_t vm_snapshot(const vm_ctx_t *vm, void *v_buf, size_t max_size);
/// Restores the VM state from a snapshot buffer.
/// The function specified by @a f_restore_dev is called for every device
/// separately in order to restore the connected device context (see
/// #cb_restore_dev_t).
/// @param f_restore_dev Device restoration callback.
/// @param v_buf Snapshot buffer.
/// @param max_size Size of @a v_buf.
/// @param[out] out_size Size of the restored snapshot in bytes.
/// @returns A newly created VM context structure with the state restored from
/// the buffer @a v_buf.
vm_ctx_t *vm_restore(cb_restore_dev_t f_restore_dev, const void *v_buf,
                     size_t max_size, size_t *out_size);
/// @}

/// Connects a device described by @a dev_desc to the @a vm context.
/// See #busctl_connect_dev().
vm_err_t vm_connect_dev(vm_ctx_t *vm, const dev_desc_t *dev_desc, void *ctx);

/// Performs a VM state step.
/// See #cpu_step().
void vm_step(vm_ctx_t *vm);

#ifdef __cplusplus
}
#endif
