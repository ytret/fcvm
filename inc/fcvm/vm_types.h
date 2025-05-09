/**
 * @file vm_types.h
 * Type definitions used in the API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <fcvm/vm_err.h>

#define VM_MAX_ADDR 0xFFFFFFFF

typedef uint32_t vm_addr_t;
typedef struct busctl_dev_ctx busctl_dev_ctx_t;

typedef vm_err_t (*mem_read_u8_cb)(void *ctx, vm_addr_t addr, uint8_t *out);
typedef vm_err_t (*mem_read_u32_cb)(void *ctx, vm_addr_t addr, uint32_t *out);
typedef vm_err_t (*mem_write_u8_cb)(void *ctx, vm_addr_t addr, uint8_t val);
typedef vm_err_t (*mem_write_u32_cb)(void *ctx, vm_addr_t addr, uint32_t val);

/// Memory interface used by the CPU.
typedef struct {
    mem_read_u8_cb read_u8;
    mem_read_u32_cb read_u32;
    mem_write_u8_cb write_u8;
    mem_write_u32_cb write_u32;
} mem_if_t;

/// @addtogroup snapshots
/// @{

/**
 * Device callback that calculates the device context snapshot size.
 * @param ctx Context passed to #vm_connect_dev() or #busctl_connect_dev().
 * @returns The required size in bytes of the snapshot buffer needed to store
 * the device context snapshot.
 */
typedef size_t (*cb_snapshot_size_dev_t)(const void *ctx);
/**
 * Device callback that writes the device context snapshot into @a v_buf.
 * @param ctx      Device context that was passed to #vm_connect_dev() or
 *                 #busctl_connect_dev().
 * @param v_buf    Snapshot buffer.
 * @param max_size Size of @a v_buf.
 * @returns Size of the saved snapshot in bytes.
 */
typedef size_t (*cb_snapshot_dev_t)(const void *ctx, void *v_buf,
                                    size_t max_size);
/**
 * Callback that restores the device context @a ctx for the device class @a
 * dev_class.
 * @param dev_class @ref dev_desc_t.dev_class "Device class" that was passed to
 *                  #vm_connect_dev() or #busctl_connect_dev().
 * @param ctx       The bus controller device context that needs to be restored.
 * @param v_buf     Snapshot buffer.
 * @param max_size  Size of @a v_buf.
 * @returns The number of bytes consumed from the snapshot buffer to restore the
 * device context.
 */
typedef size_t (*cb_restore_dev_t)(uint8_t dev_class, busctl_dev_ctx_t *ctx,
                                   void *v_buf, size_t max_size);
/// @}

/// Device descriptor.
typedef struct {
    uint8_t dev_class;
    vm_addr_t region_size;
    mem_if_t mem_if;

    cb_snapshot_size_dev_t f_snapshot_size;
    cb_snapshot_dev_t f_snapshot;
} dev_desc_t;
