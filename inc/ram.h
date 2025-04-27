#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t size;
    uint8_t *buf;
} ram_t;

void ram_init(ram_t *ram, uint32_t size);
ram_t *ram_state_load(void *buf, size_t buf_size);
void ram_deinit(void *ctx_ram);

size_t ram_state_size(const void *ctx_ram);
size_t ram_state_save(const void *ctx_ram, void *buf, size_t buf_size);

uint8_t ram_read_u8(void *ctx_ram, uint32_t addr);
void ram_write_u8(void *ctx_ram, uint32_t addr, uint8_t val);
uint32_t ram_read_u32(void *ctx_ram, uint32_t addr);
void ram_write_u32(void *ctx_ram, uint32_t addr, uint32_t val);
