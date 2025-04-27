#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VM_NUM_GP_REGS      8
#define VM_MAX_MMIO_REGIONS 32

typedef size_t (*mmio_state_size_cb)(const void *ctx);
typedef size_t (*mmio_state_save_cb)(const void *ctx, void *buf,
                                     size_t buf_size);

typedef uint8_t (*mmio_read_u8_cb)(void *ctx, uint32_t addr);
typedef void (*mmio_write_u8_cb)(void *ctx, uint32_t addr, uint8_t val);
typedef uint32_t (*mmio_read_u32_cb)(void *ctx, uint32_t addr);
typedef void (*mmio_write_u32_cb)(void *ctx, uint32_t addr, uint32_t val);
typedef void (*mmio_deinit_cb)(void *ctx);

typedef enum {
    MMIO_RAM,
} mmio_type_t;

typedef struct {
    bool loaded;
    mmio_type_t type;
    uint32_t base;
    uint32_t size;

    void *ctx;
    mmio_read_u8_cb pf_read_u8;
    mmio_write_u8_cb pf_write_u8;
    mmio_read_u32_cb pf_read_u32;
    mmio_write_u32_cb pf_write_u32;
    mmio_deinit_cb pf_deinit;

    mmio_state_size_cb pf_state_size;
    mmio_state_save_cb pf_state_save;
} mmio_t;

typedef struct {
    // CPU state.
    uint32_t regs_gp[VM_NUM_GP_REGS];
    uint32_t reg_pc;
    uint32_t reg_sp;
    uint8_t flags;
    uint64_t cycles;
    uint8_t interrupt_depth;

    // Memory IO devices.
    mmio_t mmio_regions[VM_MAX_MMIO_REGIONS];
    uint32_t mmio_count;
} vm_state_t;

vm_state_t *vm_new(void);
vm_state_t *vm_load(void *buf, size_t buf_size);
void vm_free(vm_state_t *vm);

size_t vm_state_size(const vm_state_t *vm);
void vm_state_save(const vm_state_t *vm, void *buf, size_t buf_size);

void vm_step(vm_state_t *vm);
void vm_map_device(vm_state_t *vm, const mmio_t *mmio);

uint8_t vm_read_u8(vm_state_t *vm, uint32_t addr);
void vm_write_u8(vm_state_t *vm, uint32_t addr, uint8_t byte);
uint32_t vm_read_u32(vm_state_t *vm, uint32_t addr);
void vm_write_u32(vm_state_t *vm, uint32_t addr, uint32_t dword);
