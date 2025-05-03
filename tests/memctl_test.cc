#include <gtest/gtest.h>

#include "fake_mem.h"
#include "memctl.h"

#define TEST_MMIO1_START 0x0000'0000
#define TEST_MMIO1_SIZE  8

#define TEST_MMIO_GAP    20
#define TEST_MMIO2_START (TEST_MMIO1_START + TEST_MMIO1_SIZE + TEST_MMIO_GAP)
#define TEST_MMIO2_SIZE  100

// Region 2 should not start at 0, because it's used to test reads/writes to a
// region that doesn't start at 0.
static_assert(TEST_MMIO2_START > 0);

class MemCtlTest : public testing::Test {
  protected:
    MemCtlTest() {
        memctl = memctl_new();

        mmio1_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE);
        mmio1_reg = {
            .start = TEST_MMIO1_START,
            .end = TEST_MMIO1_START + TEST_MMIO1_SIZE,
            .ctx = &mmio1_dev->mem_if,
            .mem_if = mmio1_dev->mem_if,
        };

        mmio2_dev = new FakeMem(0x0000'0000, TEST_MMIO2_SIZE);
        mmio2_reg = {
            .start = TEST_MMIO2_START,
            .end = TEST_MMIO2_START + TEST_MMIO2_SIZE,
            .ctx = &mmio2_dev->mem_if,
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

TEST_F(MemCtlTest, MapEndR2ToStartR1) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //        [  R1  ]
    // [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.end = mmio_reg.start;
    mmio_reg.start = mmio_reg.end - 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapStartR2ToEndR1) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    //        [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.end;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapWithOverlapFails1) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    //   [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start += 5;
    mmio_reg.end += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapWithOverlapFails2) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    //   [ R2 ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapWithOverlapFails3) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    // [   R2   ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.start;
    mmio_reg.end += 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapWithOverlapFails4) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //   [  R1  ]
    // [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start -= 5;
    mmio_reg.end -= 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapWithOverlapFails5) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //   [  R1  ]
    // [   R2   ]
    mmio_reg.start = 1024;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_NONE);
    mmio_reg.start -= 10;
    mmio_reg.end = mmio_reg.end;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err.type, VM_ERR_MEM_USED);
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

TEST_F(MemCtlTest, Region2ReadU8) {
    vm_addr_t rel_addr;
    uint8_t exp_byte;
    uint8_t act_byte;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err.type, VM_ERR_NONE);

    // Read from the start.
    rel_addr = 0x0000'0000;
    exp_byte = 0xDE;
    mmio2_dev->write(rel_addr, &exp_byte, 1);
    memctl_read_u8(memctl, TEST_MMIO2_START + rel_addr, &act_byte);
    EXPECT_EQ(act_byte, exp_byte);

    // Read from the middle.
    rel_addr = TEST_MMIO2_SIZE / 2;
    exp_byte = 0xAD;
    mmio2_dev->write(rel_addr, &exp_byte, 1);
    memctl_read_u8(memctl, TEST_MMIO2_START + rel_addr, &act_byte);
    EXPECT_EQ(act_byte, exp_byte);

    // Read from the end.
    rel_addr = TEST_MMIO2_SIZE - 1;
    exp_byte = 0xBE;
    mmio2_dev->write(rel_addr, &exp_byte, 1);
    memctl_read_u8(memctl, TEST_MMIO2_START + rel_addr, &act_byte);
    EXPECT_EQ(act_byte, exp_byte);
}

TEST_F(MemCtlTest, Region2WriteU8) {
    vm_addr_t rel_addr;
    uint8_t exp_byte;
    uint8_t act_byte;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err.type, VM_ERR_NONE);

    // Read from the start.
    rel_addr = 0x0000'0000;
    exp_byte = 0xDE;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);

    // Read from the middle.
    rel_addr = TEST_MMIO2_SIZE / 2;
    exp_byte = 0xAD;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);

    // Read from the end.
    rel_addr = TEST_MMIO2_SIZE - 1;
    exp_byte = 0xBE;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);
}
