#include <gtest/gtest.h>

#include <assert.h>
#include <string.h>

#include "testcommon/fake_mem.h"

std::map<void *, FakeMem *> FakeMem::_ctx_to_this;

FakeMem::FakeMem(vm_addr_t abase, vm_addr_t aend, bool fail_on_wrong_access) {
    assert(aend > abase);

    vm_addr_t size = aend - abase;
    bytes = new uint8_t[size];
    base = abase;
    end = aend;

    memset(bytes, 0xFF, size);

    mem_if.read_u8 = read_u8;
    mem_if.read_u32 = read_u32;
    mem_if.write_u8 = write_u8;
    mem_if.write_u32 = write_u32;
    this->fail_on_wrong_access = fail_on_wrong_access;

    _ctx_to_this[&mem_if] = this;
}

FakeMem::~FakeMem() {
    _ctx_to_this.erase(&mem_if);
    delete[] bytes;
}

vm_err_t FakeMem::read(vm_addr_t addr, void *out_buf, size_t num_bytes) {
    vm_err_t err;
    read_impl(addr, out_buf, num_bytes, &err);
    return err;
}

vm_err_t FakeMem::write(vm_addr_t addr, const void *buf, size_t num_bytes) {
    vm_err_t err;
    write_impl(addr, buf, num_bytes, &err);
    return err;
}

FakeMem *FakeMem::find_obj(void *ctx) {
    assert(_ctx_to_this.contains(ctx));
    return _ctx_to_this[ctx];
}

vm_err_t FakeMem::read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
    FakeMem *obj = find_obj(ctx);
    return obj->read(addr, out, 1);
}

vm_err_t FakeMem::read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
    FakeMem *obj = find_obj(ctx);
    return obj->read(addr, out, 4);
}

vm_err_t FakeMem::write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
    FakeMem *obj = find_obj(ctx);
    return obj->write(addr, &val, 1);
}

vm_err_t FakeMem::write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
    FakeMem *obj = find_obj(ctx);
    return obj->write(addr, &val, 4);
}

void FakeMem::read_impl(vm_addr_t addr, void *out_buf, size_t num_bytes,
                        vm_err_t *out_err) {
    if (base <= addr && (addr + num_bytes) <= end) {
        vm_addr_t rel_addr = addr - base;
        memcpy(out_buf, &bytes[rel_addr], num_bytes);
        *out_err = {.type = VM_ERR_NONE};
    } else if (fail_on_wrong_access) {
        EXPECT_LE(base, addr);
        EXPECT_LE(addr + num_bytes, end);
        *out_err = {.type = VM_ERR_BAD_MEM};
    } else {
        fprintf(stderr,
                "FakeMem: tried to read %zu bytes at bad address 0x%08X\n",
                num_bytes, addr);
        *out_err = {.type = VM_ERR_BAD_MEM};
    }
}

void FakeMem::write_impl(vm_addr_t addr, const void *buf, size_t num_bytes,
                         vm_err_t *out_err) {
    if (base <= addr && (addr + num_bytes) <= end) {
        vm_addr_t rel_addr = addr - base;
        memcpy(&bytes[rel_addr], buf, num_bytes);
        *out_err = {.type = VM_ERR_NONE};
    } else if (fail_on_wrong_access) {
        EXPECT_LE(base, addr);
        EXPECT_LE(addr + num_bytes, end);
        *out_err = {.type = VM_ERR_BAD_MEM};
    } else {
        fprintf(stderr,
                "FakeMem: tried to write %zu bytes at bad address 0x%08X\n",
                num_bytes, addr);
        *out_err = {.type = VM_ERR_BAD_MEM};
    }
}
