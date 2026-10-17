#include <gtest/gtest.h>

#include "testcommon/fake_mem.h"
#include <fcvm/memctl.h>

#define TEST_MMIO1_START 0x0000'0000
#define TEST_MMIO1_SIZE  8

#define TEST_MMIO_GAP    20
#define TEST_MMIO2_START (TEST_MMIO1_START + TEST_MMIO1_SIZE + TEST_MMIO_GAP)
#define TEST_MMIO2_SIZE  100

#define TEST_MMIO3_START TEST_MMIO1_START
#define TEST_MMIO3_SIZE  TEST_MMIO1_SIZE

#define TEST_MMIO_AZ_START 0
#define TEST_MMIO_AZ_SIZE  0xFFFFFFFF

// Region 2 should not start at 0, because it's used to test reads/writes to a
// region that doesn't start at 0.
static_assert(TEST_MMIO2_START > 0);

class AllZeroMem {
  public:
    static constexpr uint8_t dev_class = 0x02;

    vm_addr_t base;
    vm_addr_t end;
    mem_if_t mem_if;

    AllZeroMem(vm_addr_t abase, vm_addr_t aend) {
        base = abase;
        end = aend;

        mem_if.read_u8 = nullptr;
        mem_if.read_u32 = read_u32;
        mem_if.write_u8 = nullptr;
        mem_if.write_u32 = write_u32;
    }

    dev_desc_t dev_desc() const {
        return dev_desc_t{
            .dev_class = dev_class,
            .region_size = end - base,
            .mem_if = mem_if,
            .f_snapshot_size = nullptr,
            .f_snapshot = nullptr,
        };
    }

  private:
    static vm_err_t read_u32(void *ctx, vm_addr_t addr, uint32_t *out) {
        // Always read 0.
        (void)ctx;
        (void)addr;
        *out = 0;
        return VM_ERR_NONE;
    }

    static vm_err_t write_u32(void *ctx, vm_addr_t addr, uint32_t val) {
        // Ignore without an error.
        (void)ctx;
        (void)addr;
        (void)val;
        return VM_ERR_NONE;
    }
};

class MemCtlTest : public testing::Test {
  protected:
    MemCtlTest() {
        memctl = memctl_new();

        // Set the `fail_on_wrong_access` option to `true`, because memctl must
        // not propagate wrong reads/writes to the memory interfaces.

        mmio1_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE, true);
        mmio1_reg = {
            .start = TEST_MMIO1_START,
            .end_incl = TEST_MMIO1_START + TEST_MMIO1_SIZE - 1,
            .ctx = &mmio1_dev->mem_if,
            .mem_if = mmio1_dev->mem_if,
        };

        mmio2_dev = new FakeMem(0x0000'0000, TEST_MMIO2_SIZE, true);
        mmio2_reg = {
            .start = TEST_MMIO2_START,
            .end_incl = TEST_MMIO2_START + TEST_MMIO2_SIZE - 1,
            .ctx = &mmio2_dev->mem_if,
            .mem_if = mmio2_dev->mem_if,
        };

        mmio3_dev = new FakeMem(0x0000'0000, TEST_MMIO1_SIZE, true);
        mmio3_reg = {
            .start = TEST_MMIO3_START,
            .end_incl = TEST_MMIO3_START + TEST_MMIO3_SIZE - 1,
            .ctx = &mmio2_dev->mem_if,
            .mem_if = mmio2_dev->mem_if,
        };
        mmio3_reg.mem_if.read_u8 = nullptr;
        mmio3_reg.mem_if.write_u8 = nullptr;
        mmio3_reg.mem_if.read_u32 = nullptr;
        mmio3_reg.mem_if.write_u32 = nullptr;

        mmio_az_dev = new AllZeroMem(0x0000'0000, TEST_MMIO_AZ_SIZE);
        mmio_az_reg = {
            .start = TEST_MMIO_AZ_START,
            .end_incl = TEST_MMIO_AZ_START + TEST_MMIO_AZ_SIZE - 1,
            .ctx = &mmio_az_dev->mem_if,
            .mem_if = mmio_az_dev->mem_if,
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

    /// A memory-mapped fake memory device that lacks the u32 r/w capability.
    /// It's a copy of #mmio1_dev, except that the #mem_if_t function pointers
    /// have been set to `nullptr`.
    FakeMem *mmio3_dev;
    mmio_region_t mmio3_reg;
    /// @}

    AllZeroMem *mmio_az_dev;
    mmio_region_t mmio_az_reg;
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
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.end_incl = mmio_reg.start - 1;
    mmio_reg.start = mmio_reg.end_incl - 9;
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
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.end_incl + 1;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
}

TEST_F(MemCtlTest, MapWithOverlapR2StartsInR1Fails1) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    //   [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start += 5;
    mmio_reg.end_incl += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapWithOverlapR2StartsInR1Fails2) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    //   [ R2 ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start += 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapRegionStartingAtExistingRegionStartFails) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [  R1  ]
    // [   R2   ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start = mmio_reg.start;
    mmio_reg.end_incl += 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapRegionEndingInExistingRegionFails1) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //   [  R1  ]
    // [  R2  ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start -= 5;
    mmio_reg.end_incl -= 5;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapRegionEndingInExistingRegionFails2) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //   [  R1  ]
    // [   R2   ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start -= 10;
    mmio_reg.end_incl = mmio_reg.end_incl;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapRegionCoveringExistingRegionFails) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    //   [ R1 ]
    // [   R2   ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 9;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start -= 10;
    mmio_reg.end_incl += 10;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_MEM_USED);
}

TEST_F(MemCtlTest, MapRegionFullyInsideExistingRegionFails) {
    vm_err_t err;
    mmio_region_t mmio_reg;
    memcpy(&mmio_reg, &mmio1_reg, sizeof(mmio_reg));

    // [   R1   ]
    //   [ R2 ]
    mmio_reg.start = 1024;
    mmio_reg.end_incl = mmio_reg.start + 511;
    err = memctl_map_region(memctl, &mmio_reg);
    EXPECT_EQ(err, VM_ERR_NONE);
    mmio_reg.start += 10;
    mmio_reg.end_incl -= 10;
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

TEST_F(MemCtlTest, RegionReadBeyondEndFails) {
    vm_addr_t rel_addr;
    uint32_t act_dword;

    vm_err_t err = memctl_map_region(memctl, &mmio2_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Read from the end dword.
    rel_addr = TEST_MMIO2_SIZE - 4;
    err = memctl_read_u32(memctl, TEST_MMIO2_START + rel_addr, &act_dword);
    EXPECT_EQ(err, VM_ERR_NONE);

    // Reading a dword anywhere from [end-3, end) should fail.
    for (vm_addr_t i = 0; i < 3; i++) {
        const vm_addr_t addr = TEST_MMIO2_START + TEST_MMIO2_SIZE - i;
        err = memctl_read_u32(memctl, addr, &act_dword);
        EXPECT_EQ(err, VM_ERR_BAD_MEM)
            << std::format("while reading address 0x{:08x}", addr);
    }
}

TEST_F(MemCtlTest, ReadBeyondAddrSpaceEndFails) {
    vm_addr_t rel_addr;
    uint32_t act_dword;

    vm_err_t err = memctl_map_region(memctl, &mmio_az_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Read from the end dword.
    rel_addr = TEST_MMIO_AZ_SIZE - 4;
    err = memctl_read_u32(memctl, TEST_MMIO_AZ_START + rel_addr, &act_dword);
    EXPECT_EQ(err, VM_ERR_NONE);

    // Reading a dword anywhere from [end-3, end) should fail.
    for (vm_addr_t i = 0; i < 3; i++) {
        const vm_addr_t addr = TEST_MMIO_AZ_START + TEST_MMIO_AZ_SIZE - i;
        err = memctl_read_u32(memctl, addr, &act_dword);
        EXPECT_EQ(err, VM_ERR_BAD_MEM)
            << std::format("while reading address 0x{:08x}", addr);
    }
}

TEST_F(MemCtlTest, WriteBeyondAddrSpaceEndFails) {
    vm_addr_t rel_addr;
    uint32_t write_dword = 0;

    vm_err_t err = memctl_map_region(memctl, &mmio_az_reg);
    ASSERT_EQ(err, VM_ERR_NONE);

    // Write to the last dword.
    rel_addr = TEST_MMIO_AZ_SIZE - 4;
    err = memctl_write_u32(memctl, TEST_MMIO_AZ_START + rel_addr, write_dword);
    EXPECT_EQ(err, VM_ERR_NONE);

    // Writing a dword anywhere from [end-3, end) should fail.
    for (vm_addr_t i = 0; i < 3; i++) {
        const vm_addr_t addr = TEST_MMIO_AZ_START + TEST_MMIO_AZ_SIZE - i;
        err = memctl_write_u32(memctl, addr, write_dword);
        EXPECT_EQ(err, VM_ERR_BAD_MEM)
            << std::format("while writing address 0x{:08x}", addr);
    }
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
        EXPECT_EQ(rest_memctl->mapped_regions[idx].end_incl,
                  memctl->mapped_regions[idx].end_incl);

        EXPECT_EQ(rest_memctl->mapped_regions[idx].ctx, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.read_u8, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.read_u32, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.write_u8, nullptr);
        EXPECT_EQ(rest_memctl->mapped_regions[idx].mem_if.write_u32, nullptr);
    }

    delete[] snapshot_buf;
}
