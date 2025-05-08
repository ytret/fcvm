#include <gtest/gtest.h>
#include <random>
#include <testcommon/fake_mem.h>
#include <testcommon/get_random.h>
#include <testcommon/get_random_prog.h>
#include <vm.h>

#define TEST_VM_SNAPSHOT_NUM_PROGS 1
#define TEST_VM_SNAPSHOT_MEM_SIZE  128

static bool g_restore_dev_ok;

static size_t restore_dev(uint8_t dev_class, busctl_dev_ctx_t *busdev_ctx,
                          void *v_buf, size_t max_size);

struct VMSnapshotParam {
    size_t num_steps;
    std::vector<uint8_t> prog;

    static VMSnapshotParam get_random_param(std::mt19937 &rng) {
        VMSnapshotParam param = {};
        // param.num_steps = get_random_imm32(rng, 0, 10);
        param.num_steps = 10;
        param.prog = get_random_prog(rng, TEST_VM_SNAPSHOT_MEM_SIZE);
        return param;
    }
};

class VMSnapshotTest : public testing::TestWithParam<VMSnapshotParam> {
  protected:
    VMSnapshotTest() {
        vm = vm_new();
        mem = new FakeMem(0, TEST_VM_SNAPSHOT_MEM_SIZE);

        dev_desc_t mem_desc = mem->dev_desc();
        vm_connect_dev(vm, &mem_desc, mem);
    }
    ~VMSnapshotTest() {
        vm_free(vm);
        delete mem;
    }

    vm_ctx_t *vm;
    FakeMem *mem;
};

TEST_P(VMSnapshotTest, SnapshotRestoreNoSegFaults) {
    auto param = GetParam();

    // Create the initial snapshot.
    size_t req_size = vm_snapshot_size(vm);
    uint8_t *snapshot = new uint8_t[req_size];
    size_t used_size = vm_snapshot(vm, snapshot, req_size);
    ASSERT_EQ(used_size, req_size) << "initial snapshot size mismatch";

    for (size_t step = 0; step < param.num_steps; step++) {
        // Restore from the last snapshot.
        g_restore_dev_ok = false;
        size_t res_size = 0;
        vm_ctx_t *res_vm =
            vm_restore(restore_dev, snapshot, req_size, &res_size);
        ASSERT_EQ(res_size, req_size)
            << "restore on step " << step
            << ": used size vs snapshot size mismatch";
        ASSERT_TRUE(g_restore_dev_ok)
            << "restore on step " << step << ": failed to restore the device";
        delete[] snapshot;

        // Create a snapshot.
        req_size = vm_snapshot_size(res_vm);
        snapshot = new uint8_t[req_size];
        used_size = vm_snapshot(vm, snapshot, req_size);
        ASSERT_EQ(used_size, req_size)
            << "snapshot on step " << step << ": used size mismatch";
        vm_free(res_vm);

        // Delete the allocated memory device.
        void *mem_ctx = res_vm->busctl->devs[0].snapshot_ctx;
        ASSERT_NE(mem_ctx, nullptr);
        delete reinterpret_cast<FakeMem *>(mem_ctx);
    }

    delete[] snapshot;
}

TEST_P(VMSnapshotTest, DISABLED_SnapshotRestoreInSeparateProcess) {
    FAIL();
}

INSTANTIATE_TEST_SUITE_P(RandomProg, VMSnapshotTest, testing::ValuesIn([&] {
                             std::vector<VMSnapshotParam> v;
                             std::mt19937 rng(TEST_RNG_SEED);
                             for (size_t idx = 0;
                                  idx < TEST_VM_SNAPSHOT_NUM_PROGS; idx++) {
                                 v.push_back(
                                     VMSnapshotParam::get_random_param(rng));
                             }
                             return v;
                         }()));

static size_t restore_dev(uint8_t dev_class, busctl_dev_ctx_t *busdev_ctx,
                          void *v_buf, size_t max_size) {
    size_t used_size = 0;
    if (dev_class == FakeMem::dev_class) {
        FakeMem *rest_mem;
        used_size += FakeMem::restore(&rest_mem, busdev_ctx, v_buf, max_size);
        g_restore_dev_ok = true;
    } else {
        g_restore_dev_ok = false;
    }
    return used_size;
}
