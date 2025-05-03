#include <gtest/gtest.h>

#include "memctl.h"

class MemCtlTest : public testing::Test {
  protected:
    MemCtlTest() {
        memctl = memctl_new();
    }
    ~MemCtlTest() {
        memctl_free(memctl);
    }

    memctl_ctx_t *memctl;
};

TEST_F(MemCtlTest, InitNoRegions) {
    EXPECT_EQ(memctl->num_mapped_regions, 0);
}

TEST_F(MemCtlTest, NoRegionReadU8) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.read_u8(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0x12);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionReadU32) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.read_u32(memctl, 0x0000'0000, &val);
    EXPECT_EQ(val, 0xDEADBEEF);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU8) {
    uint8_t val = 0x12;
    vm_err_t err = memctl->intf.write_u8(memctl, 0x0000'0000, val);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}

TEST_F(MemCtlTest, NoRegionWriteU32) {
    uint32_t val = 0xDEADBEEF;
    vm_err_t err = memctl->intf.write_u32(memctl, 0x0000'0000, val);
    EXPECT_EQ(err.type, VM_ERR_BAD_MEM);
}
