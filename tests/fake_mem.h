#pragma once

#include <assert.h>
#include <iostream>
#include <map>
#include <string.h>
#include <sys/types.h>

#include "mem.h"

class FakeMem {
  public:
    explicit FakeMem(vm_addr_t abase, vm_addr_t aend) {
        assert(aend > abase);
        vm_addr_t size = aend - abase;
        bytes = new uint8_t[size];
        base = abase;
        end = aend;

        mem_if.read_u8 = read_u8;
        mem_if.read_u32 = read_u32;
        mem_if.write_u8 = write_u8;
        mem_if.write_u32 = write_u32;

        _ctx_to_this[&mem_if] = this;
    }

    ~FakeMem() {
        _ctx_to_this.erase(&mem_if);
        delete[] bytes;
    }

    vm_err_t read(vm_addr_t addr, void *out_buf, size_t num_bytes) {
        if (base <= addr && (addr + num_bytes) <= end) {
            vm_addr_t rel_addr = addr - base;
            memcpy(out_buf, &bytes[rel_addr], num_bytes);
            return {.type = VM_ERR_NONE};
        } else {
            fprintf(stderr,
                    "FakeMem: tried to read %zu bytes at bad address 0x%08X\n",
                    num_bytes, addr);
            return {.type = VM_ERR_BAD_MEM};
        }
    }

    vm_err_t write(vm_addr_t addr, const void *buf, size_t num_bytes) {
        if (base <= addr && (addr + num_bytes) <= end) {
            vm_addr_t rel_addr = addr - base;
            memcpy(&bytes[rel_addr], buf, num_bytes);
            return {.type = VM_ERR_NONE};
        } else {
            fprintf(stderr,
                    "FakeMem: tried to write %zu bytes at bad address 0x%08X\n",
                    num_bytes, addr);
            return {.type = VM_ERR_BAD_MEM};
        }
    }

    uint8_t *bytes;
    vm_addr_t base;
    vm_addr_t end;
    mem_if_t mem_if;

  private:
    static std::map<void *, FakeMem *> _ctx_to_this;
    static FakeMem *find_obj(void *ctx) {
        assert(_ctx_to_this.contains(ctx));
        return _ctx_to_this[ctx];
    }

    static vm_err_t read_u8(void *ctx, vm_addr_t addr, uint8_t *out) {
        // std::cerr << "read_u8 " << std::hex << addr << "\n";
        FakeMem *obj = find_obj(ctx);
        return obj->read(addr, out, 1);
    }
    static vm_err_t read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
        // std::cerr << "read_u32 " << std::hex << addr << "\n";
        FakeMem *obj = find_obj(ctx);
        return obj->read(addr, out, 4);
    }
    static vm_err_t write_u8(void *ctx, vm_addr_t addr, uint8_t val) {
        // std::cerr << "write_u8 " << std::hex << addr << " " << val << "\n";
        FakeMem *obj = find_obj(ctx);
        return obj->write(addr, &val, 1);
    }
    static vm_err_t write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
        // std::cerr << "write_u32 " << std::hex << addr << " " << val << "\n";
        FakeMem *obj = find_obj(ctx);
        return obj->write(addr, &val, 4);
    }
};

std::map<void *, FakeMem *> FakeMem::_ctx_to_this;
