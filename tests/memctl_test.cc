#include <gtest/gtest.h>

#include "fake_mem.h"
#include "memctl.h"

#define TEST_MMIO1_START 0x0000'0000
#define TEST_MMIO1_SIZE  8

#define TEST_MMIO_GAP    20
#define TEST_MMIO2_START (TEST_MMIO1_START + TEST_MMIO1_SIZE + TEST_MMIO_GAP)
#define TEST_MMIO2_SIZE  100

class MemCtlTest : public testing::Test {
  protected:
    MemCtlTest() {
        memctl = memctl_new();

        mmio1_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE);
        mmio1_reg = {
            .start = TEST_MMIO1_START,
            .end = TEST_MMIO1_START + TEST_MMIO1_SIZE,
            .ctx = mmio1_dev,
            .mem_if = mmio1_dev->mem_if,
        };

        mmio2_dev = new FakeMem(0x0000'0000, TEST_MMIO2_SIZE);
        mmio2_reg = {
            .start = TEST_MMIO2_START,
            .end = TEST_MMIO2_START + TEST_MMIO2_SIZE,
            .ctx = mmio2_dev,
            .mem_if = mmio2_dev->mem_if,
        };
    }
    ~MemCtlTest() {
        memctl_free(memctl);
        delete mmio1_dev;
    }

    memctl_ctx_t *memctl;

    /**
     * @{
     * \name Fake memory-mapped devices
     * These are used to test the region mapping and read/write interface.
     */
    FakeMem *mmio1_dev;
    mmio_region_t mmio1_reg;

    FakeMem *mmio2_dev;
    mmio_region_t mmio2_reg;
    /// @}
};

TEST_F(MemCtlTest, InitNoRegions) {
    EXPECT_EQ(memctl->num_mapped_regions, 0);
}

TEST_F(MemCtlTest, NoRegionReadU8Fails) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.read_u8(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0x12);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionReadU32Fails) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.read_u32(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0xDEADBEEF);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU8Fails) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.write_u8(memctl, 0x0000'0000, val);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU32Fails) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.write_u32(memctl, 0x0000'0000, val);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, MapOneRegion) {
    vm_err_t err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapTwoRegions) {
    vm_err_t err;

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);

    err = memctl_map_region(memctl, &mmio2_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapSameRegionTwiceFails) {
    vm_err_t err;

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
}
