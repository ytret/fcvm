#include <gtest/gtest.h>

#include <assert.h>
#include <string.h>

#include <fcvm/busctl.h>
#include "testcommon/fake_mem.h"

std::map<void *, FakeMem *> FakeMem::_mmio_ctx_to_this;

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

    _mmio_ctx_to_this[&mem_if] = this;
}

FakeMem::~FakeMem() {
    _mmio_ctx_to_this.erase(&mem_if);
    delete[] bytes;
}

dev_desc_t FakeMem::dev_desc() const {
    return dev_desc_t{
        .dev_class = dev_class,
        .region_size = end - base,
        .mem_if = mem_if,
        .f_snapshot_size = snapshot_size_cb,
        .f_snapshot = snapshot_cb,
    };
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

size_t FakeMem::snapshot_size() const {
    size_t mem_size = end - base;
    return sizeof(base) + sizeof(end) + sizeof(fail_on_wrong_access) + mem_size;
}

size_t FakeMem::snapshot(void *v_buf, size_t max_size) const {
    uint8_t *buf = static_cast<uint8_t *>(v_buf);
    size_t offset = 0;

    // Write 'base', 'end', 'fail_on_wrong_access'.
    assert(offset + sizeof(base) <= max_size);
    memcpy(&buf[offset], &base, sizeof(base));
    offset += sizeof(base);
    assert(offset + sizeof(end) <= max_size);
    memcpy(&buf[offset], &end, sizeof(end));
    offset += sizeof(end);
    assert(offset + sizeof(fail_on_wrong_access) <= max_size);
    memcpy(&buf[offset], &fail_on_wrong_access, sizeof(fail_on_wrong_access));
    offset += sizeof(fail_on_wrong_access);

    // Write the memory buffer.
    size_t mem_size = end - base;
    assert(offset + mem_size <= max_size);
    memcpy(&buf[offset], bytes, mem_size);
    offset += mem_size;

    return offset;
}

size_t FakeMem::restore(FakeMem **out_fakemem,
                        struct busctl_dev_ctx *busdev_ctx, void *v_buf,
                        size_t max_size) {
    uint8_t *buf = static_cast<uint8_t *>(v_buf);
    size_t offset = 0;

    // Read 'base', 'end', 'fail_on_wrong_access'.
    vm_addr_t base;
    assert(offset + sizeof(base) <= max_size);
    memcpy(&base, &buf[offset], sizeof(base));
    offset += sizeof(base);

    vm_addr_t end;
    assert(offset + sizeof(end) <= max_size);
    memcpy(&end, &buf[offset], sizeof(end));
    offset += sizeof(end);

    bool fail_on_wrong_access;
    assert(offset + sizeof(fail_on_wrong_access) <= max_size);
    memcpy(&fail_on_wrong_access, &buf[offset], sizeof(fail_on_wrong_access));
    offset += sizeof(fail_on_wrong_access);

    // Create the new FakeMem object.
    FakeMem *fake_mem = new FakeMem(base, end, fail_on_wrong_access);
    *out_fakemem = fake_mem;

    // Copy the memory buffer.
    assert(end > base);
    size_t mem_size = end - base;
    assert(offset + mem_size <= max_size);
    memcpy(fake_mem->bytes, &buf[offset], mem_size);
    offset += mem_size;

    // Fill in the busctl dev ctx.
    if (busdev_ctx) {
        busdev_ctx->mmio.ctx = &fake_mem->mem_if;
        busdev_ctx->mmio.mem_if = fake_mem->mem_if;
        busdev_ctx->snapshot_ctx = fake_mem;
        busdev_ctx->f_snapshot_size = snapshot_size_cb;
        busdev_ctx->f_snapshot = snapshot_cb;
    }

    return offset;
}

FakeMem *FakeMem::find_obj_by_mmio_ctx(void *ctx) {
    assert(_mmio_ctx_to_this.contains(ctx));
    return _mmio_ctx_to_this[ctx];
}

vm_err_t FakeMem::read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
    FakeMem *obj = find_obj_by_mmio_ctx(ctx);
    return obj->read(addr, out, 1);
}

vm_err_t FakeMem::read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
    FakeMem *obj = find_obj_by_mmio_ctx(ctx);
    return obj->read(addr, out, 4);
}

vm_err_t FakeMem::write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
    FakeMem *obj = find_obj_by_mmio_ctx(ctx);
    return obj->write(addr, &val, 1);
}

vm_err_t FakeMem::write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
    FakeMem *obj = find_obj_by_mmio_ctx(ctx);
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

size_t FakeMem::snapshot_size_cb(const void *snapshot_ctx) {
    const FakeMem *obj = reinterpret_cast<const FakeMem *>(snapshot_ctx);
    return obj->snapshot_size();
}

size_t FakeMem::snapshot_cb(const void *snapshot_ctx, void *v_buf,
                            size_t max_size) {
    const FakeMem *obj = reinterpret_cast<const FakeMem *>(snapshot_ctx);
    return obj->snapshot(v_buf, max_size);
}
