#include <random>

#include <boost/asio/read.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <gtest/gtest.h>

#include <testcommon/fake_mem.h>
#include <testcommon/get_random.h>
#include <testcommon/get_random_prog.h>
#include <vm.h>

#define TEST_VM_SNAPSHOT_NUM_PROGS 100
#define TEST_VM_SNAPSHOT_NUM_STEPS 100

#define TEST_VM_SNAPSHOT_PROG_START (CPU_IVT_ADDR + CPU_IVT_SIZE)
#define TEST_VM_SNAPSHOT_PROG_SIZE  1024
#define TEST_VM_SNAPSHOT_MEM_SIZE                                              \
    (TEST_VM_SNAPSHOT_PROG_SIZE + TEST_VM_SNAPSHOT_PROG_SIZE)

static bool g_restore_dev_ok;

static size_t restore_dev(uint8_t dev_class, busctl_dev_ctx_t *busdev_ctx,
                          void *v_buf, size_t max_size);

struct VMSnapshotParam {
    size_t num_steps;
    std::vector<uint8_t> prog;
    const boost::filesystem::path test_proc_path =
        "./tests/progs/cpu_snapshot_step";

    static VMSnapshotParam get_random_param(std::mt19937 &rng) {
        VMSnapshotParam param = {};
        param.num_steps = TEST_VM_SNAPSHOT_NUM_STEPS;
        param.prog = get_random_prog(rng, TEST_VM_SNAPSHOT_PROG_SIZE);
        param.prog.resize(TEST_VM_SNAPSHOT_PROG_SIZE);
        return param;
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const VMSnapshotParam &param) {
        os << "{ random program, " << param.prog.size() << " bytes, "
           << param.num_steps << " steps }";
        return os;
    }
};

class VMSnapshotTest : public testing::TestWithParam<VMSnapshotParam> {
  protected:
    VMSnapshotTest() {
        auto param = GetParam();

        vm = vm_new();
        mem = new FakeMem(CPU_IVT_ADDR, TEST_VM_SNAPSHOT_MEM_SIZE);

        // Build an IVT.
        std::array<vm_addr_t, CPU_IVT_NUM_ENTRIES> ivt;
        static_assert(sizeof(vm_addr_t) == CPU_IVT_ENTRY_SIZE);
        std::for_each(ivt.begin(), ivt.end(), [](vm_addr_t &isr_addr) {
            isr_addr = TEST_VM_SNAPSHOT_PROG_START;
        });
        mem->write(CPU_IVT_ADDR, ivt.data(), CPU_IVT_ENTRY_SIZE * ivt.size());

        // Write the program.
        mem->write(TEST_VM_SNAPSHOT_PROG_START, param.prog.data(),
                   param.prog.size());

        dev_desc_t mem_desc = mem->dev_desc();
        vm_connect_dev(vm, &mem_desc, mem);
    }
    ~VMSnapshotTest() {
        vm_free(vm);
        delete mem;
    }

    static int run_test_proc(const std::vector<uint8_t> &inbytes,
                             std::vector<uint8_t> &outbytes,
                             std::vector<char> &errbytes) {
        namespace asio = boost::asio;
        namespace bp = boost::process::v2;

        auto param = GetParam();

        asio::io_context ioctx;
        asio::writable_pipe inpipe{ioctx};
        asio::readable_pipe outpipe{ioctx};
        asio::readable_pipe errpipe{ioctx};
        bp::process proc(
            ioctx, param.test_proc_path, {},
            bp::process_stdio{.in = inpipe, .out = outpipe, .err = errpipe});

        inpipe.write_some(asio::buffer(inbytes.data(), inbytes.size()));
        inpipe.close();

        int ec = proc.wait();

        // Read from stdout a size prefix (N), then read N bytes.
        if (ec == 0) {
            size_t out_size = 0;
            size_t num_read =
                asio::read(outpipe, asio::buffer(&out_size, sizeof(out_size)));
            if (num_read != sizeof(out_size)) {
                fprintf(stderr,
                        "run_test_proc: could not read snapshot size\n");
                abort();
            }
            outbytes.resize(out_size);
            num_read = asio::read(
                outpipe, asio::buffer(outbytes.data(), outbytes.size()));
            if (num_read != outbytes.size()) {
                fprintf(stderr,
                        "run_test_proc: stdout size mismatch: expected %zu, "
                        "got %zu\n",
                        out_size, num_read);
                abort();
            }
        }

        // Read from stderr until EOF.
        size_t num_read;
        std::array<uint8_t, 4096> err_buf;
        boost::system::error_code err_ec;
        while ((num_read = errpipe.read_some(asio::buffer(err_buf), err_ec)) >
               0) {
            errbytes.insert(errbytes.end(), err_buf.data(),
                            err_buf.data() + num_read);
        }
        if (err_ec && err_ec != asio::error::eof) {
            fprintf(stderr, "run_test_proc: failed to read from stderr: %s\n",
                    err_ec.to_string().c_str());
            abort();
        }

        return ec;
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

        // Do a step.
        vm_step(res_vm);

        // Create a snapshot.
        req_size = vm_snapshot_size(res_vm);
        snapshot = new uint8_t[req_size];
        used_size = vm_snapshot(res_vm, snapshot, req_size);
        ASSERT_EQ(used_size, req_size)
            << "snapshot on step " << step << ": used size mismatch";

        // Delete the allocated memory device.
        void *mem_ctx = res_vm->busctl->devs[0].snapshot_ctx;
        ASSERT_NE(mem_ctx, nullptr);
        delete reinterpret_cast<FakeMem *>(mem_ctx);

        // Delete the restored VM.
        vm_free(res_vm);
    }

    delete[] snapshot;
}

TEST_P(VMSnapshotTest, SnapshotRestoreInSeparateProcess) {
    // Same as above, except that the process of restoring from a snapshot,
    // doing a VM step, and creating a snapshot is duplicated in a separate
    // process, and the resulting snapshots are expected to be equal.
    //
    // The snapshot which is used in the gtest process is named 'this_snap'.
    // The snapshot which is used and produced by the launched process is named
    // 'proc_snap'.

    auto param = GetParam();

    // Create the initial gtest-process snapshot.
    size_t req_size = vm_snapshot_size(vm);
    std::vector<uint8_t> this_snap(req_size);
    vm_snapshot(vm, this_snap.data(), req_size);

    // Copy the initial snapshot to be used for stdin/stdout of the launched
    // process.
    std::vector<uint8_t> proc_snap = this_snap;

    for (size_t step = 0; step < param.num_steps; step++) {
        // Restore from the last snapshot.
        g_restore_dev_ok = false;
        size_t res_size = 0;
        vm_ctx_t *res_vm = vm_restore(restore_dev, this_snap.data(),
                                      this_snap.size(), &res_size);

        // Do a step.
        vm_step(res_vm);

        // Create a snapshot.
        req_size = vm_snapshot_size(res_vm);
        this_snap.resize(req_size);
        vm_snapshot(vm, this_snap.data(), req_size);

        // Delete the memory device allocated during restoral.
        void *mem_ctx = res_vm->busctl->devs[0].snapshot_ctx;
        ASSERT_NE(mem_ctx, nullptr);
        delete reinterpret_cast<FakeMem *>(mem_ctx);

        // Delete the restored VM.
        vm_free(res_vm);

        // Do the same steps in a separate process.
        std::vector<uint8_t> proc_out_snap;
        std::vector<char> proc_stderr;
        int ec = run_test_proc(proc_snap, proc_out_snap, proc_stderr);
        EXPECT_EQ(ec, 0)
            << "launched process exited with non-zero code, stderr:\n"
            << std::string(proc_stderr.begin(), proc_stderr.end());
    }
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
