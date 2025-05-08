#pragma once

#include <map>

#include "vm_types.h"

class FakeMem {
  public:
    explicit FakeMem(vm_addr_t abase, vm_addr_t aend,
                     bool fail_on_wrong_access = false);
    ~FakeMem();

    vm_err_t read(vm_addr_t addr, void *out_buf, size_t num_bytes);
    vm_err_t write(vm_addr_t addr, const void *buf, size_t num_bytes);

    uint8_t *bytes;
    vm_addr_t base;
    vm_addr_t end;
    mem_if_t mem_if;
    bool fail_on_wrong_access;

    static constexpr uint8_t dev_class = 0x01;

    size_t snapshot_size() const;
    size_t snapshot(void *v_buf, size_t max_size) const;
    static size_t restore(FakeMem **out_ctx, void *v_buf, size_t max_size);

  private:
    static std::map<void *, FakeMem *> _ctx_to_this;
    static FakeMem *find_obj(void *ctx);
    static const FakeMem *find_obj(const void *ctx);

    static vm_err_t read_u8(void *ctx, vm_addr_t addr, uint8_t *out);
    static vm_err_t read_u32(void *ctx, vm_addr_t addr, uint32_t *out);
    static vm_err_t write_u8(void *ctx, vm_addr_t addr, uint8_t val);
    static vm_err_t write_u32(void *ctx, vm_addr_t addr, uint32_t val);

    void read_impl(vm_addr_t addr, void *out_buf, size_t num_bytes,
                   vm_err_t *out_err);
    void write_impl(vm_addr_t addr, const void *buf, size_t num_bytes,
                    vm_err_t *out_err);

    static size_t snapshot_size_cb(const void *snapshot_ctx);
    static size_t snapshot_cb(const void *snapshot_ctx, void *v_buf,
                              size_t max_size);
    static size_t restore_cb(void **snapshot_ctx, void **mem_ctx,
                             mem_if_t *mem_if, void *v_buf, size_t max_size);
};
