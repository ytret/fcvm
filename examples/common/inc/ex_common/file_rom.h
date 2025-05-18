/**
 * @file file_rom.h
 * Example read-only memory device API.
 *
 * A read-only memory device that is filled with bytes read from a file upon its
 * initial creation. Subsequent snapshots and restorings do not change the
 * bytes.
 */

#pragma once

#include <fcvm/vm_types.h>

#define FILE_ROM_DEV_CLASS 0x02

/// File ROM device context.
typedef struct {
    size_t size;
    uint8_t *buf;

    dev_desc_t desc;
} file_rom_ctx_t;

/**
 * Creates a new ROM device filled with contents of the file at @a path.
 *
 * @param path Path to the backing file.
 *
 * @returns
 * A pointer to the newly created context structure on success; `NULL` on error.
 */
file_rom_ctx_t *file_rom_new(const char *path);
void file_rom_free(file_rom_ctx_t *ctx);

size_t file_rom_snapshot_size(const void *v_ctx);
size_t file_rom_snapshot(const void *v_ctx, void *v_buf, size_t max_size);
file_rom_ctx_t *file_rom_restore(const void *v_buf, size_t max_size);

vm_err_t file_rom_read_u8(void *v_ctx, vm_addr_t addr, uint8_t *out);
vm_err_t file_rom_read_u32(void *v_ctx, vm_addr_t addr, uint32_t *out);
