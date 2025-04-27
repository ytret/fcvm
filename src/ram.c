#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "ram.h"

static void prv_ram_check_access(ram_t *ram, uint32_t addr,
                                 uint32_t access_size);

ram_t *ram_init(uint32_t size) {
    ram_t *ram = malloc(sizeof(ram_t));
    D_ASSERT(ram != NULL);
    D_ASSERT(size > 0);

    memset(ram, 0, sizeof(ram_t));
    ram->size = size;
    ram->buf = malloc(size);
    D_ASSERT(ram->buf != NULL);

    return ram;
}

ram_t *ram_load(void *v_buf, size_t buf_size) {
    ram_t *ram = (ram_t *)malloc(sizeof(ram_t));
    D_ASSERT(ram != NULL);

    D_ASSERT(v_buf != NULL);
    uint8_t *buf = (uint8_t *)v_buf;

    D_ASSERT(buf_size >= sizeof(ram->size));
    memcpy(&ram->size, &buf[0], sizeof(ram->size));

    ram->buf = malloc(ram->size);
    D_ASSERT(ram->buf != NULL);
    memcpy(ram->buf, &buf[sizeof(ram->size)], ram->size);

    return ram;
}

void ram_free(void *ctx_ram) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    D_ASSERT(ram->buf != NULL);
    free(ram->buf);
}

size_t ram_state_size(const void *ctx_ram) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    return sizeof(ram->size) + ram->size;
}

size_t ram_state_save(const void *ctx_ram, void *v_buf, size_t buf_size) {
    D_ASSERT(ctx_ram != NULL);
    D_ASSERT(v_buf != NULL);
    D_ASSERT(buf_size >= ram_state_size(ctx_ram));
    ram_t *ram = (ram_t *)ctx_ram;
    uint8_t *buf = (uint8_t *)v_buf;

    memcpy(&buf[0], &ram->size, sizeof(ram->size));
    memcpy(&buf[sizeof(ram->size)], ram->buf, ram->size);
    return sizeof(ram->size) + ram->size;
}

uint8_t ram_read_u8(void *ctx_ram, uint32_t addr) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    prv_ram_check_access(ram, addr, 1);
    return ram->buf[addr];
}

void ram_write_u8(void *ctx_ram, uint32_t addr, uint8_t val) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    prv_ram_check_access(ram, addr, 1);
    ram->buf[addr] = val;
}

uint32_t ram_read_u32(void *ctx_ram, uint32_t addr) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    prv_ram_check_access(ram, addr, 4);
    uint32_t dword;
    memcpy(&dword, &ram->buf[addr], 4);
    return dword;
}

void ram_write_u32(void *ctx_ram, uint32_t addr, uint32_t val) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    prv_ram_check_access(ram, addr, 4);
    memcpy(&ram->buf[addr], &val, 4);
}

static void prv_ram_check_access(ram_t *ram, uint32_t addr,
                                 uint32_t access_size) {
    // TODO: raise an exception in place of asserts
    D_ASSERT(access_size == 1 || access_size == 4);

    if (access_size == 1) {
        D_ASSERTMF((addr + 1) <= ram->size,
                   "byte 0x%08X is out of bounds for RAM with %u dwords", addr,
                   ram->size);
    } else if (access_size == 4) {
        // Commented out because cpu.c reads misaligned dwords.
        /*
        D_ASSERTMF(addr % 4 == 0,
                   "addr 0x%08X is misaligned for access size %u", addr,
                   access_size);
        */
        D_ASSERTMF((addr + 4) <= ram->size,
                   "dword 0x%08X is out of bounds for RAM with %u dwords", addr,
                   ram->size);
    }
}
