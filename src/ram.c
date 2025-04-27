#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "ram.h"

static void prv_ram_check_access(ram_t *ram, uint32_t addr,
                                 uint32_t access_size);

void ram_init(ram_t *ram, uint32_t size) {
    D_ASSERT(ram != NULL);
    D_ASSERT(size > 0);

    memset(ram, 0, sizeof(ram_t));
    ram->size = size;
    ram->buf = malloc(size);
    D_ASSERT(ram->buf != NULL);
}

void ram_deinit(void *ctx_ram) {
    D_ASSERT(ctx_ram != NULL);
    ram_t *ram = (ram_t *)ctx_ram;
    D_ASSERT(ram->buf != NULL);
    free(ram->buf);
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
