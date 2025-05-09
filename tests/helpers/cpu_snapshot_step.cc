#include <iostream>
#include <vector>

#include <fcvm/vm.h>
#include <testcommon/fake_mem.h>

static bool g_restore_dev_ok = false;

static size_t restore_dev(uint8_t dev_class, busctl_dev_ctx_t *busdev_ctx,
                          void *v_buf, size_t max_size);

std::vector<uint8_t> read_stdin_bytes() {
    std::istreambuf_iterator<char> begin{std::cin}, end{};
    std::vector<uint8_t> bytes;
    std::copy(begin, end, std::back_inserter(bytes));
    return bytes;
}

int main() {
    std::cerr << "Reading state from standard input\n";
    std::vector<uint8_t> state_in = read_stdin_bytes();
    std::cerr << "Read " << state_in.size() << " bytes\n";

    size_t rest_size = 0;
    vm_ctx_t *vm =
        vm_restore(restore_dev, state_in.data(), state_in.size(), &rest_size);
    if (!g_restore_dev_ok) {
        fprintf(stderr, "failed to restore FakeMem");
        abort();
    }

    vm_step(vm);

    size_t snapshot_size = vm_snapshot_size(vm);
    std::vector<uint8_t> snapshot_bytes;
    snapshot_bytes.resize(snapshot_size);

    size_t used_size =
        vm_snapshot(vm, snapshot_bytes.data(), snapshot_bytes.size());
    if (used_size != snapshot_size) {
        fprintf(stderr,
                "size mismatch during creating a snapshot: requested %zu, used "
                "%zu\n",
                snapshot_size, used_size);
        abort();
    }

    size_t out_size = snapshot_bytes.size();
    fprintf(stderr, "Writing snapshot (%zu bytes) to stdout\n", out_size);
    std::cout.write(reinterpret_cast<const char *>(&out_size),
                    sizeof(out_size));
    std::cout.write(reinterpret_cast<const char *>(snapshot_bytes.data()),
                    snapshot_bytes.size());
    std::cout.flush();

    return 0;
}

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
