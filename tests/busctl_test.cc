#include <gtest/gtest.h>

#include "busctl.h"
#include "common.h"
#include "memctl.h"

class BusCtlTest : public testing::Test {
  protected:
    BusCtlTest() {
        intctl = intctl_new();
        memctl = mem_new();
        busctl = busctl_new(memctl, intctl);
    }
    ~BusCtlTest() {
        busctl_free(busctl);
        mem_free(memctl);
        intctl_free(intctl);
    }

    mem_ctx_t *memctl;
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
            .region_size = (vm_addr_t)idx_dev,
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
