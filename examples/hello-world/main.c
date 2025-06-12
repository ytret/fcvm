#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include <ex_common/file_rom.h>
#include <ex_common/print_dev.h>
#include <fcvm/vm.h>

#ifndef EX_HELLO_WORLD_GUEST
#    error "path to the guest program (EX_HELLO_WORLD_GUEST) is not provided"
#endif

static atomic_bool g_running = true;
static atomic_int g_caught_signum = 0;

static vm_ctx_t *g_vm = NULL;
static file_rom_ctx_t *g_file_rom = NULL;
static print_dev_ctx_t *g_print_dev = NULL;

static int install_sigint_handler(void);
static void sigint_handler(int signum);

static int init(void);
static int vm_loop(void);
static void cleanup(void);

int main(void) {
    int err_code = 0;
    if ((err_code = install_sigint_handler()) != 0) {
        cleanup();
        return err_code;
    }
    if ((err_code = init()) != 0) {
        cleanup();
        return err_code;
    }

    fprintf(stderr, "%s: starting the VM, press Ctrl-C to exit\n", __func__);
    if ((err_code = vm_loop()) != 0) {
        cleanup();
        return err_code;
    }

    fprintf(stderr, "%s: caught signal %u\n", __func__, g_caught_signum);
    cleanup();
    return 0;
}

static int install_sigint_handler(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "%s: failed to install a SIGINT handler: %s\n",
                __func__, strerror(errno));
        return 1;
    }
    return 0;
}

static void sigint_handler(int signum) {
    g_running = false;
    g_caught_signum = signum;
}

static int init(void) {
    g_file_rom = file_rom_new(EX_HELLO_WORLD_GUEST);
    if (!g_file_rom) {
        fprintf(stderr, "%s: file_rom_new() returned NULL\n", __func__);
        return 1;
    }

    g_print_dev = print_dev_new();
    if (!g_print_dev) {
        fprintf(stderr, "%s: print_dev_new() returned NULL\n", __func__);
        return 1;
    }

    g_vm = vm_new();
    vm_err_t err = vm_connect_dev(g_vm, &g_file_rom->desc, g_file_rom);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "%s: could not connect file_rom, error %u\n", __func__,
                err);
        return 1;
    }
    err = vm_connect_dev(g_vm, &g_print_dev->desc, g_print_dev);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "%s: could not connect print_dev, error %u\n", __func__,
                err);
        return 1;
    }

    return 0;
}

static int vm_loop(void) {
    while (g_running) {
        vm_step(g_vm);

        if (g_vm->cpu->num_nested_exc != 0) {
            fprintf(stderr, "%s: unexpected CPU exception, shutting down\n",
                    __func__);
            return 1;
        }
    }
    return 0;
}

static void cleanup(void) {
    fprintf(stderr, "%s: freeing resources\n", __func__);
    if (g_vm) { vm_free(g_vm); }
    if (g_file_rom) { file_rom_free(g_file_rom); }
    if (g_print_dev) { print_dev_free(g_print_dev); }
}
