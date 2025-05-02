#include <stdlib.h>
#include <string.h>

#include "debugm.h"
#include "mem.h"

mem_ctx_t *mem_new(void) {
    mem_ctx_t *mem = malloc(sizeof(*mem));
    D_ASSERT(mem);
    memset(mem, 0, sizeof(*mem));

    mem->intf.read_u8 = mem_read_u8;
    mem->intf.read_u32 = mem_read_u32;
    mem->intf.write_u8 = mem_write_u8;
    mem->intf.write_u32 = mem_write_u32;

    return mem;
}

void mem_free(mem_ctx_t *mem) {
    D_ASSERT(mem);
    free(mem);
}

vm_err_t mem_map_dev(mem_ctx_t *mem, const mem_dev_t *dev) {
    (void)mem;
    (void)dev;
    D_TODO();
}

vm_err_t mem_read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t mem_read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
    (void)ctx;
    (void)addr;
    (void)out;
    D_TODO();
}

vm_err_t mem_write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}

vm_err_t mem_write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
    (void)ctx;
    (void)addr;
    (void)val;
    D_TODO();
}
