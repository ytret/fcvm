#include <gtest/gtest.h>

#include "busctl.h"
#include "memctl.h"

/// A test memory mapped device.
struct TestDevice {
    static constexpr uint8_t dev_class = 0x02;
    static constexpr vm_addr_t region_size = 4;

    uint32_t contained_dword = 0xDEADBEEF;

    busctl_req_t build_req() {
        return busctl_req_t{
            .dev_class = dev_class,
            .region_size = region_size,
            .ctx = static_cast<void *>(this),
            .mem_if = {.read_u8 = nullptr,
                       .read_u32 = read_u32,
                       .write_u8 = nullptr,
                       .write_u32 = write_u32},
        };
    }

    void read_u32_impl(vm_addr_t addr, uint32_t *out_u32, vm_err_t *out_err) {
        ASSERT_LT(addr, region_size);
        ASSERT_NE(out_u32, nullptr);
        ASSERT_NE(out_err, nullptr);
        *out_u32 = contained_dword;
        out_err->type = VM_ERR_NONE;
    }

    void write_u32_impl(vm_addr_t addr, uint32_t u32, vm_err_t *out_err) {
        ASSERT_LT(addr, region_size);
        ASSERT_NE(out_err, nullptr);
        contained_dword = u32;
        out_err->type = VM_ERR_NONE;
    }

  private:
    static vm_err_t read_u32(void *v_ctx, vm_addr_t addr, uint32_t *out_u32) {
        TestDevice *dev = reinterpret_cast<TestDevice *>(v_ctx);
        vm_err_t err;
        dev->read_u32_impl(addr, out_u32, &err);
        return err;
    }

    static vm_err_t write_u32(void *v_ctx, vm_addr_t addr, uint32_t u32) {
        TestDevice *dev = reinterpret_cast<TestDevice *>(v_ctx);
        vm_err_t err;
        dev->write_u32_impl(addr, u32, &err);
        return err;
    }
};

class BusCtlTest : public testing::Test {
  protected:
    BusCtlTest() {
        intctl = intctl_new();
        memctl = memctl_new();
        busctl = busctl_new(memctl, intctl);
    }
    ~BusCtlTest() {
        busctl_free(busctl);
        memctl_free(memctl);
        intctl_free(intctl);
    }

    memctl_ctx_t *memctl;
    intctl_ctx_t *intctl;
    busctl_ctx_t *busctl;
};

TEST_F(BusCtlTest, RegisterDevice) {
    uint32_t mem_ctx = 0xDEADBEEF;
    mem_if_t mem_if = {
        .read_u8 = NULL,
        .read_u32 = NULL,
        .write_u8 = NULL,
        .write_u32 = NULL,
    };
    busctl_req_t req = {
        .dev_class = 0xAA,
        .region_size = 10,
        .ctx = &mem_ctx,
        .mem_if = mem_if,
    };

    const busctl_dev_ctx_t *dev_ctx = nullptr;
    vm_err_t err = busctl_reg_dev(busctl, &req, &dev_ctx);
    ASSERT_EQ(err.type, VM_ERR_NONE);

    ASSERT_NE(dev_ctx, nullptr);
    EXPECT_EQ(dev_ctx->dev_class, req.dev_class);
    EXPECT_EQ(dev_ctx->mmio.ctx, req.ctx);
    EXPECT_EQ(dev_ctx->mmio.end - dev_ctx->mmio.start, req.region_size);
}

TEST_F(BusCtlTest, RegisterMaxDevices) {
    ASSERT_GT(BUS_MAX_DEVS, 0);

    uint32_t mem_ctx = 0xDEADBEEF;
    mem_if_t mem_if;
    busctl_req_t req;
    const busctl_dev_ctx_t *dev_ctx;
    for (size_t idx_dev = 0; idx_dev < BUS_MAX_DEVS; idx_dev++) {
        mem_if = {
            .read_u8 = NULL,
            .read_u32 = NULL,
            .write_u8 = NULL,
            .write_u32 = NULL,
        };
        req = {
            .dev_class = (uint8_t)idx_dev,
            .region_size = 10 + (vm_addr_t)idx_dev,
            .ctx = &mem_ctx,
            .mem_if = mem_if,
        };

        vm_err_t err = busctl_reg_dev(busctl, &req, &dev_ctx);
        ASSERT_EQ(err.type, VM_ERR_NONE);
        ASSERT_NE(dev_ctx, nullptr);
    }

    vm_err_t err = busctl_reg_dev(busctl, &req, &dev_ctx);
    ASSERT_EQ(err.type, VM_ERR_BUS_NO_FREE_SLOT);
}

TEST_F(BusCtlTest, TestDeviceReadWriteU8Fails) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    busctl_req_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_reg_dev(busctl, &req, &dev_ctx);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Try to read u8.
    uint8_t byte = 0xAA;
    err = memctl_read_u8(memctl, dev_ctx->mmio.start, &byte);
    EXPECT_EQ(err.type, VM_ERR_MEM_BAD_OP);
    EXPECT_EQ(byte, 0xAA);

    // Try to write u8.
    byte = 0xAA;
    err = memctl_write_u8(memctl, dev_ctx->mmio.start, byte);
    EXPECT_EQ(err.type, VM_ERR_MEM_BAD_OP);
    EXPECT_EQ(byte, 0xAA);
}

TEST_F(BusCtlTest, TestDeviceReadWriteU32) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    busctl_req_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_reg_dev(busctl, &req, &dev_ctx);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Try to read u32.
    const uint32_t dev_dword = test_dev.contained_dword;
    uint32_t read_dword = 0xCC00FFEE;
    ASSERT_NE(dev_dword, read_dword)
        << "update the initial value of variable 'read_dword' to be different "
           "than the initial value of TestDevice::contained_dword";
    err = memctl_read_u32(memctl, dev_ctx->mmio.start, &read_dword);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    EXPECT_EQ(read_dword, dev_dword);

    // Try to write u32.
    uint32_t write_dword = 0xCAFEBABE;
    ASSERT_NE(write_dword, read_dword)
        << "update the initial value of variable 'write_dword' to be different "
           "than the initial value of TestDevice::contained_dword";
    err = memctl_write_u32(memctl, dev_ctx->mmio.start, write_dword);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    EXPECT_EQ(test_dev.contained_dword, write_dword);
}
