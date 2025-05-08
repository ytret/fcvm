#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <testcommon/fake_mem.h>

#define TEST_MEM_SIZE 128
#define TEST_MEM_BASE 0x0001'0100
#define TEST_MEM_END  (TEST_MEM_BASE + TEST_MEM_SIZE)

class FakeMemTest : public testing::Test {};

TEST_F(FakeMemTest, SnapshotRestore) {
    FakeMem *orig_mem = new FakeMem(TEST_MEM_BASE, TEST_MEM_END, true);
    memset(orig_mem->bytes, 0xAE, TEST_MEM_SIZE);

    size_t snapshot_size = orig_mem->snapshot_size();
    uint8_t *snapshot_bytes = new uint8_t[snapshot_size];
    size_t used_size = orig_mem->snapshot(snapshot_bytes, snapshot_size);
    EXPECT_EQ(used_size, snapshot_size);

    FakeMem *rest_mem = nullptr;
    size_t rest_size =
        FakeMem::restore(&rest_mem, NULL, snapshot_bytes, used_size);
    EXPECT_EQ(rest_size, used_size);
    ASSERT_NE(rest_mem, nullptr);

    EXPECT_EQ(rest_mem->base, orig_mem->base);
    EXPECT_EQ(rest_mem->end, orig_mem->end);
    EXPECT_EQ(rest_mem->fail_on_wrong_access, orig_mem->fail_on_wrong_access);

    size_t mem_size = rest_mem->end - rest_mem->base;
    EXPECT_THAT(std::make_tuple(rest_mem->bytes, mem_size),
                testing::Each(0xAE));

    delete orig_mem;
    delete[] snapshot_bytes;
}
