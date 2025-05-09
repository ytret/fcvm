#include <gtest/gtest.h>

#include <fcvm/busctl.h>
#include <fcvm/memctl.h>

/// A test memory mapped device.
struct TestDevice {
    static constexpr uint8_t dev_class = 0x02;
    static constexpr vm_addr_t region_size = 4;

    uint32_t contained_dword = 0xDEADBEEF;

    dev_desc_t build_req() {
        return dev_desc_t{
            .dev_class = dev_class,
            .region_size = region_size,
            .mem_if = {.read_u8 = nullptr,
                       .read_u32 = read_u32,
                       .write_u8 = nullptr,
                       .write_u32 = write_u32},
            .f_snapshot_size = nullptr,
            .f_snapshot = nullptr,
        };
    }

    void read_u32_impl(vm_addr_t addr, uint32_t *out_u32, vm_err_t *out_err) {
        ASSERT_LT(addr, region_size);
        ASSERT_NE(out_u32, nullptr);
        ASSERT_NE(out_err, nullptr);
        *out_u32 = contained_dword;
        *out_err = VM_ERR_NONE;
    }

    void write_u32_impl(vm_addr_t addr, uint32_t u32, vm_err_t *out_err) {
        ASSERT_LT(addr, region_size);
        ASSERT_NE(out_err, nullptr);
        contained_dword = u32;
        *out_err = VM_ERR_NONE;
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
    dev_desc_t req = {
        .dev_class = 0xAA,
        .region_size = 10,
        .mem_if = mem_if,
        .f_snapshot_size = nullptr,
        .f_snapshot = nullptr,
    };

    const busctl_dev_ctx_t *dev_ctx = nullptr;
    vm_err_t err = busctl_connect_dev(busctl, &req, &mem_ctx, &dev_ctx);
    ASSERT_EQ(err, VM_ERR_NONE);

    ASSERT_NE(dev_ctx, nullptr);
    EXPECT_EQ(dev_ctx->dev_class, req.dev_class);
    EXPECT_EQ(dev_ctx->mmio.ctx, &mem_ctx);
    EXPECT_EQ(dev_ctx->mmio.end - dev_ctx->mmio.start, req.region_size);
}

TEST_F(BusCtlTest, RegisterMaxDevices) {
    ASSERT_GT(BUS_MAX_DEVS, 0);
    ASSERT_GE(MEMCTL_MAX_REGIONS, BUS_MAX_DEVS + 1)
        << "Not enough regions to map " << BUS_MAX_DEVS
        << " devices plus the bus controller MMIO";

    uint32_t mem_ctx = 0xDEADBEEF;
    mem_if_t mem_if;
    dev_desc_t req;
    const busctl_dev_ctx_t *dev_ctx;
    for (size_t idx_dev = 0; idx_dev < BUS_MAX_DEVS; idx_dev++) {
        mem_if = {.read_u8 = NULL,
                  .read_u32 = NULL,
                  .write_u8 = NULL,
                  .write_u32 = NULL};
        req = {
            .dev_class = (uint8_t)idx_dev,
            .region_size = 10 + (vm_addr_t)idx_dev,
            .mem_if = mem_if,
            .f_snapshot_size = nullptr,
            .f_snapshot = nullptr,
        };

        vm_err_t err = busctl_connect_dev(busctl, &req, &mem_ctx, &dev_ctx);
        ASSERT_EQ(err, VM_ERR_NONE);
        ASSERT_NE(dev_ctx, nullptr);
    }

    vm_err_t err = busctl_connect_dev(busctl, &req, &mem_ctx, &dev_ctx);
    ASSERT_EQ(err, VM_ERR_BUS_NO_FREE_SLOT);
}

TEST_F(BusCtlTest, TestDeviceReadWriteU8Fails) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    dev_desc_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_connect_dev(busctl, &req, &test_dev, &dev_ctx);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Try to read u8.
    uint8_t byte = 0xAA;
    err = memctl_read_u8(memctl, dev_ctx->mmio.start, &byte);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);
    EXPECT_EQ(byte, 0xAA);

    // Try to write u8.
    byte = 0xAA;
    err = memctl_write_u8(memctl, dev_ctx->mmio.start, byte);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);
    EXPECT_EQ(byte, 0xAA);
}

TEST_F(BusCtlTest, TestDeviceReadWriteU32) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    dev_desc_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_connect_dev(busctl, &req, &test_dev, &dev_ctx);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Try to read u32.
    const uint32_t dev_dword = test_dev.contained_dword;
    uint32_t read_dword = 0xCC00FFEE;
    ASSERT_NE(dev_dword, read_dword)
        << "update the initial value of variable 'read_dword' to be different "
           "than the initial value of TestDevice::contained_dword";
    err = memctl_read_u32(memctl, dev_ctx->mmio.start, &read_dword);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_EQ(read_dword, dev_dword);

    // Try to write u32.
    uint32_t write_dword = 0xCAFEBABE;
    ASSERT_NE(write_dword, read_dword)
        << "update the initial value of variable 'write_dword' to be different "
           "than the initial value of TestDevice::contained_dword";
    err = memctl_write_u32(memctl, dev_ctx->mmio.start, write_dword);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_EQ(test_dev.contained_dword, write_dword);
}

TEST_F(BusCtlTest, TestDeviceRaiseIRQ) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    dev_desc_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_connect_dev(busctl, &req, &test_dev, &dev_ctx);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Raise the assigned IRQ line.
    uint8_t pending_irq = 0xFF;
    ASSERT_NE(pending_irq, dev_ctx->irq_line)
        << "update the initial value of variable 'pending_irq' to something "
           "different, because busctl assigned this IRQ line to the device";
    EXPECT_FALSE(intctl_has_pending_irqs(intctl));
    err = intctl_raise_irq_line(intctl, dev_ctx->irq_line);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_TRUE(intctl_has_pending_irqs(intctl));
    EXPECT_TRUE(intctl_get_pending_irq(intctl, &pending_irq));
    EXPECT_EQ(pending_irq, dev_ctx->irq_line);
}

TEST_F(BusCtlTest, MMIORegSlotStatusNoDev) {
    constexpr vm_addr_t slot_status_addr = BUS_MMIO_START;
    uint32_t slot_status = 0xDEADBEEF;

    vm_err_t err =
        memctl->intf.read_u32(memctl, slot_status_addr, &slot_status);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_EQ(slot_status, 0);
}

TEST_F(BusCtlTest, MMIORegSlotStatusOneDev) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    dev_desc_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_connect_dev(busctl, &req, &test_dev, &dev_ctx);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Read the slot status register.
    constexpr vm_addr_t slot_status_addr = BUS_MMIO_START;
    uint32_t slot_status = 0xDEADBEEF;
    err = memctl->intf.read_u32(memctl, slot_status_addr, &slot_status);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_EQ(slot_status, (1 << dev_ctx->bus_slot));
}

TEST_F(BusCtlTest, MMIORegSlotStatusTwoDevs) {
    TestDevice dev1;
    TestDevice dev2;
    vm_err_t err;

    // Register the test device #1.
    dev_desc_t req1 = dev1.build_req();
    const busctl_dev_ctx_t *ctx1 = nullptr;
    err = busctl_connect_dev(busctl, &req1, &dev1, &ctx1);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(ctx1, nullptr);

    // Register the test device #2.
    dev_desc_t req2 = dev2.build_req();
    const busctl_dev_ctx_t *ctx2 = nullptr;
    err = busctl_connect_dev(busctl, &req2, &dev2, &ctx2);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(ctx2, nullptr);

    // Ensure that they have different bus slots.
    EXPECT_NE(ctx1->bus_slot, ctx2->bus_slot);

    // Read the slot status register.
    constexpr vm_addr_t slot_status_addr = BUS_MMIO_START;
    uint32_t slot_status = 0xDEADBEEF;
    err = memctl->intf.read_u32(memctl, slot_status_addr, &slot_status);
    EXPECT_EQ(err, VM_ERR_NONE);
    EXPECT_EQ(slot_status, (1 << ctx1->bus_slot) | (1 << ctx2->bus_slot));
}

TEST_F(BusCtlTest, MMIORegDevDesc) {
    TestDevice test_dev;
    vm_err_t err;

    // Register the test device.
    dev_desc_t req = test_dev.build_req();
    const busctl_dev_ctx_t *dev_ctx = nullptr;
    err = busctl_connect_dev(busctl, &req, &test_dev, &dev_ctx);
    EXPECT_EQ(err, VM_ERR_NONE);
    ASSERT_NE(dev_ctx, nullptr);

    // Read the device slot register.
    const vm_addr_t desc_addr = BUS_MMIO_START + BUS_MMIO_DEV_DESC_START +
                                BUS_MMIO_DEV_DESC_SIZE * dev_ctx->bus_slot;
    uint32_t desc[3];
    static_assert(BUS_MMIO_DEV_DESC_SIZE == 12,
                  "device descriptor size changed, update the test");
    err = memctl->intf.read_u32(memctl, desc_addr + 0, &desc[0]);
    EXPECT_EQ(err, VM_ERR_NONE);
    err = memctl->intf.read_u32(memctl, desc_addr + 4, &desc[1]);
    EXPECT_EQ(err, VM_ERR_NONE);
    err = memctl->intf.read_u32(memctl, desc_addr + 8, &desc[2]);
    EXPECT_EQ(err, VM_ERR_NONE);

    // DWORD #1 - mapped region start.
    EXPECT_EQ(desc[0], dev_ctx->mmio.start);
    // DWORD #2 - mapped region end.
    EXPECT_EQ(desc[1], dev_ctx->mmio.end);
    // DWORD #3 - device class and IRQ line.
    EXPECT_EQ(desc[2], (dev_ctx->dev_class << 8) | dev_ctx->irq_line);
}
