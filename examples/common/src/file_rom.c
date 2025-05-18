#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <ex_common/file_rom.h>

static off_t prv_get_file_size(const char *path);
static bool prv_read_file(const char *path, uint8_t *buf, size_t buf_size);

file_rom_ctx_t *file_rom_new(const char *path) {
    file_rom_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) { return NULL; }
    memset(ctx, 0, sizeof(*ctx));

    off_t file_size = prv_get_file_size(path);
    if (file_size == -1) {
        free(ctx);
        return NULL;
    }

    uint8_t *buf = malloc(file_size);
    if (!buf) { return NULL; }
    if (!prv_read_file(path, buf, file_size)) { return NULL; }

    ctx->buf = buf;
    ctx->size = file_size;

    ctx->desc.dev_class = FILE_ROM_DEV_CLASS;
    ctx->desc.region_size = file_size;
    ctx->desc.mem_if.read_u8 = file_rom_read_u8;
    ctx->desc.mem_if.read_u32 = file_rom_read_u32;
    ctx->desc.mem_if.write_u8 = NULL; // writes are not supported
    ctx->desc.mem_if.write_u32 = NULL;
    ctx->desc.f_snapshot_size = file_rom_snapshot_size;
    ctx->desc.f_snapshot = file_rom_snapshot;

    fprintf(stderr, "file_rom: loaded %s (%ld bytes)\n", path, file_size);
    return ctx;
}

void file_rom_free(file_rom_ctx_t *ctx) {
    assert(ctx);
    assert(ctx->buf);
    free(ctx->buf);
    free(ctx);
}

size_t file_rom_snapshot_size(const void *v_ctx) {
    (void)v_ctx;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

size_t file_rom_snapshot(const void *v_ctx, void *v_buf, size_t max_size) {
    (void)v_ctx;
    (void)v_buf;
    (void)max_size;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

file_rom_ctx_t *file_rom_restore(const void *v_buf, size_t max_size) {
    (void)v_buf;
    (void)max_size;
    fprintf(stderr, "TODO: %s\n", __func__);
    abort();
}

vm_err_t file_rom_read_u8(void *v_ctx, vm_addr_t addr, uint8_t *out) {
    assert(v_ctx);
    file_rom_ctx_t *ctx = v_ctx;
    assert(ctx->buf);

    assert((addr + 1) <= ctx->size);
    *out = ctx->buf[addr];

    return VM_ERR_NONE;
}

vm_err_t file_rom_read_u32(void *v_ctx, vm_addr_t addr, uint32_t *out) {
    assert(v_ctx);
    file_rom_ctx_t *ctx = v_ctx;
    assert(ctx->buf);

    assert((addr + 4) <= ctx->size);
    memcpy(out, &ctx->buf[addr], 4);

    return VM_ERR_NONE;
}

/**
 * Returns the size of the file at @a path.
 * @param path Path to the file.
 * @returns Size of the file on success or `-1` on error.
 */
static off_t prv_get_file_size(const char *path) {
    struct stat st;
    int ec = stat(path, &st);
    if (ec != 0) {
        fprintf(stderr, "file_rom: stat returned %d: %s\n", ec,
                strerror(errno));
        return -1;
    }
    if (st.st_size < 0) {
        fprintf(stderr, "file_rom: st_size is negative: %ld\n", st.st_size);
        return -1;
    }
    return st.st_size;
}

/**
 * Reads @a buf_size bytes from file at @a path into @a buf.
 *
 * @param path     Path to the file.
 * @param buf      Buffer to read file bytes into.
 * @param buf_size Size of @a buf.
 *
 * @returns
 * `true` if exactly @a buf_size were read into @a buf from the file, otherwise
 * `false`.
 */
static bool prv_read_file(const char *path, uint8_t *buf, size_t buf_size) {
    FILE *h_file = fopen(path, "r");
    if (!h_file) {
        printf("fopen returned NULL: %s\n", strerror(errno));
        return false;
    }

    size_t num_read = fread(buf, 1, buf_size, h_file);
    if (num_read != buf_size) {
        printf("fread read %zu bytes instead of %zu: %s\n", num_read, buf_size,
               strerror(errno));
        fclose(h_file);
        return false;
    }

    int ec = fclose(h_file);
    if (ec != 0) {
        printf("fclose returned %d: %s\n", ec, strerror(errno));
        return false;
    }

    return true;
}
