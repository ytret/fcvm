#include <gtest/gtest.h>

#include <fcvm/memctl.h>
#include "testcommon/fake_mem.h"

#define TEST_MMIO1_START 0x0000'0000
#define TEST_MMIO1_SIZE  8

#define TEST_MMIO_GAP    20
#define TEST_MMIO2_START (TEST_MMIO1_START + TEST_MMIO1_SIZE + TEST_MMIO_GAP)
#define TEST_MMIO2_SIZE  100

#define TEST_MMIO3_START TEST_MMIO1_START
#define TEST_MMIO3_SIZE  TEST_MMIO1_SIZE

// Region 2 should not start at 0, because it's used to test reads/writes to a
// region that doesn't start at 0.
static_assert(TEST_MMIO2_START > 0);

class MemCtlTest : public testing::Test {
  protected:
    MemCtlTest() {
        memctl = memctl_new();

        // Set the `fail_on_wrong_access` option to `true`, because memctl must
        // not propagate wrong reads/writes to the memory interfaces.

        mmio1_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE, true);
        mmio1_reg = {
            .start = TEST_MMIO1_START,
            .end = TEST_MMIO1_START + TEST_MMIO1_SIZE,
            .ctx = &mmio1_dev->mem_if,
            .mem_if = mmio1_dev->mem_if,
        };

        mmio2_dev = new FakeMem(0x0000'0000, TEST_MMIO2_SIZE, true);
        mmio2_reg = {
            .start = TEST_MMIO2_START,
            .end = TEST_MMIO2_START + TEST_MMIO2_SIZE,
            .ctx = &mmio2_dev->mem_if,
            .mem_if = mmio2_dev->mem_if,
        };

        mmio3_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE, true);
        mmio3_reg = {
            .start = TEST_MMIO3_START,
            .end = TEST_MMIO3_START + TEST_MMIO3_SIZE,
            .ctx = &mmio2_dev->mem_if,
            .mem_if = mmio2_dev->mem_if,
        };
        mmio3_reg.mem_if.read_u8 = nullptr;
        mmio3_reg.mem_if.write_u8 = nullptr;
        mmio3_reg.mem_if.read_u32 = nullptr;
        mmio3_reg.mem_if.write_u32 = nullptr;
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

    /// A memory-mapped fake memory device that lacks the u32 r/w capability.
    /// It's a copy of #mmio1_dev, except that the #mem_if_t function pointers
    /// have been set to `nullptr`.
    FakeMem *mmio3_dev;
    mmio_region_t mmio3_reg;
    /// @}
};

TEST_F(MemCtlTest, InitNoRegions) {
    EXPECT_EQ(memctl->num_mapped_regions, 0);
}

TEST_F(MemCtlTest, MapOneRegion) {
    vm_err_t err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapTwoRegions) {
    vm_err_t err;

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err, VM_ERR_NONE);

    err = memctl_map_region(memctl, &mmio2_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapSameRegionTwiceFails) {
    vm_err_t err;

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err, VM_ERR_NONE);

    err = memctl_map_region(memctl, &mmio1_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.end = mmio_reg.start;
    mmio_reg.start = mmio_reg.end - 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.end;
    mmio_reg.end = mmio_reg.start + 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start += 5;
    mmio_reg.end += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.start;
    mmio_reg.end += 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start -= 5;
    mmio_reg.end -= 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
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
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start -= 10;
    mmio_reg.end = mmio_reg.end;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, NoRegionReadU8Fails) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.read_u8(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0x12);
    EXPECT_EQ(err, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionReadU32Fails) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.read_u32(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0xDEADBEEF);
    EXPECT_EQ(err, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU8Fails) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.write_u8(memctl, 0x0000'0000, val);
    EXPECT_EQ(err, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU32Fails) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.write_u32(memctl, 0x0000'0000, val);
    EXPECT_EQ(err, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, Region2ReadU8) {
    vm_addr_t rel_addr;
    uint8_t exp_byte;
    uint8_t act_byte;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

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

TEST_F(MemCtlTest, Region2ReadU32) {
    vm_addr_t rel_addr;
    uint32_t exp_dword;
    uint32_t act_dword;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Read from the start.
    rel_addr = 0x0000'0000;
    exp_dword = 0xDEADBEEF;
    mmio2_dev->write(rel_addr, &exp_dword, 4);
    memctl_read_u32(memctl, TEST_MMIO2_START + rel_addr, &act_dword);
    EXPECT_EQ(act_dword, exp_dword);

    // Read from the middle.
    rel_addr = TEST_MMIO2_SIZE / 2;
    exp_dword = 0xCAFEBABE;
    mmio2_dev->write(rel_addr, &exp_dword, 4);
    memctl_read_u32(memctl, TEST_MMIO2_START + rel_addr, &act_dword);
    EXPECT_EQ(act_dword, exp_dword);

    // Read from the end dword.
    rel_addr = TEST_MMIO2_SIZE - 4;
    exp_dword = 0xCC00FFEE;
    mmio2_dev->write(rel_addr, &exp_dword, 4);
    memctl_read_u32(memctl, TEST_MMIO2_START + rel_addr, &act_dword);
    EXPECT_EQ(act_dword, exp_dword);

    // Reading a dword anywhere from [end-3, end) should fail.
    for (vm_addr_t i = 0; i < 3; i++) {
        err = memctl_read_u32(memctl, TEST_MMIO2_START + TEST_MMIO2_SIZE - i,
                              &act_dword);
        EXPECT_EQ(err, VM_ERR_BAD_MEM);
    }
}

TEST_F(MemCtlTest, Region2WriteU8) {
    vm_addr_t rel_addr;
    uint8_t exp_byte;
    uint8_t act_byte;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Write at the start.
    rel_addr = 0x0000'0000;
    exp_byte = 0xDE;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);

    // Write in the middle.
    rel_addr = TEST_MMIO2_SIZE / 2;
    exp_byte = 0xAD;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);

    // Write the last byte.
    rel_addr = TEST_MMIO2_SIZE - 1;
    exp_byte = 0xBE;
    memctl_write_u8(memctl, TEST_MMIO2_START + rel_addr, exp_byte);
    mmio2_dev->read(rel_addr, &act_byte, 1);
    EXPECT_EQ(act_byte, exp_byte);
}

TEST_F(MemCtlTest, Region2WriteU32) {
    vm_addr_t rel_addr;
    uint32_t exp_dword;
    uint32_t act_dword;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Write at the start.
    rel_addr = 0x0000'0000;
    exp_dword = 0xDEADBEEF;
    memctl_write_u32(memctl, TEST_MMIO2_START + rel_addr, exp_dword);
    mmio2_dev->read(rel_addr, &act_dword, 4);
    EXPECT_EQ(act_dword, exp_dword);

    // Write in the middle.
    rel_addr = TEST_MMIO2_SIZE / 2;
    exp_dword = 0xCAFEBABE;
    memctl_write_u32(memctl, TEST_MMIO2_START + rel_addr, exp_dword);
    mmio2_dev->read(rel_addr, &act_dword, 4);
    EXPECT_EQ(act_dword, exp_dword);

    // Write the last dword.
    rel_addr = TEST_MMIO2_SIZE - 4;
    exp_dword = 0xCC00FFEE;
    memctl_write_u32(memctl, TEST_MMIO2_START + rel_addr, exp_dword);
    mmio2_dev->read(rel_addr, &act_dword, 4);
    EXPECT_EQ(act_dword, exp_dword);

    // Writing a dword anywhere at [end-3, end) should fail.
    for (vm_addr_t i = 0; i < 3; i++) {
        err = memctl_write_u32(memctl, TEST_MMIO2_START + TEST_MMIO2_SIZE - i,
                               exp_dword);
        EXPECT_EQ(err, VM_ERR_BAD_MEM);
    }
}

TEST_F(MemCtlTest, Region3CannotReadWriteU8) {
    vm_addr_t rel_addr;
    uint8_t byte = 0xAE;

    vm_err_t err = memctl_map_region(memctl, &mmio3_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Read
    rel_addr = TEST_MMIO3_SIZE / 2;
    err = memctl_read_u8(memctl, TEST_MMIO3_START + rel_addr, &byte);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);

    // Write
    rel_addr = TEST_MMIO3_SIZE / 2;
    err = memctl_write_u32(memctl, TEST_MMIO3_START + rel_addr, byte);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);
}

TEST_F(MemCtlTest, Region3CannotReadWriteU32) {
    vm_addr_t rel_addr;
    uint32_t dword = 0xDEADBEEF;

    vm_err_t err = memctl_map_region(memctl, &mmio3_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Read
    rel_addr = TEST_MMIO3_SIZE / 2;
    err = memctl_read_u32(memctl, TEST_MMIO3_START + rel_addr, &dword);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);

    // Write
    rel_addr = TEST_MMIO3_SIZE / 2;
    memctl_write_u32(memctl, TEST_MMIO3_START + rel_addr, dword);
    EXPECT_EQ(err, VM_ERR_MEM_BAD_OP);
}

TEST_F(MemCtlTest, SnapshotRestore) {
    static_assert(SN_MEMCTL_CTX_VER == 1);

    size_t snapshot_size = memctl_snapshot_size();
    uint8_t *snapshot_buf = new uint8_t[snapshot_size];
    size_t used_size = memctl_snapshot(memctl, snapshot_buf, snapshot_size);
    EXPECT_EQ(used_size, snapshot_size);

    size_t rest_size = 0;
    memctl_ctx_t *rest_memctl =
        memctl_restore(snapshot_buf, used_size, &rest_size);
    EXPECT_EQ(rest_size, used_size);

    EXPECT_NE(rest_memctl->intf.read_u8, nullptr);
    EXPECT_NE(rest_memctl->intf.read_u32, nullptr);
    EXPECT_NE(rest_memctl->intf.write_u8, nullptr);
    EXPECT_NE(rest_memctl->intf.write_u32, nullptr);

    ASSERT_EQ(rest_memctl->num_mapped_regions, memctl->num_mapped_regions);
    for (size_t idx = 0; idx < rest_memctl->num_mapped_regions; idx++) {
        EXPECT_EQ(rest_memctl->used_regions[idx], memctl->used_regions[idx]);

        EXPECT_EQ(rest_memctl->mapped_regions[idx].start,
                  memctl->mapped_regions[idx].start);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].end,
                  memctl->mapped_regions[idx].end);

        EXPECT_EQ(rest_memctl->mapped_regions[idx].ctx, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.read_u8, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.read_u32, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.write_u8, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.write_u32, nullptr);
    }

    delete[] snapshot_buf;
}
