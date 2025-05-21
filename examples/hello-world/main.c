#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include <ex_common/file_rom.h>
#include <ex_common/print_dev.h>
#include <fcvm/vm.h>

#ifndef EX_HELLO_WORLD_GUEST
#error "path to the guest program binary (EX_HELLO_WORLD_GUEST) is not provided"
#endif

static atomic_bool g_running = true;
static atomic_int g_caught_signum = 0;

static vm_ctx_t *g_vm = NULL;
static file_rom_ctx_t *g_file_rom = NULL;
static print_dev_ctx_t *g_print_dev = NULL;

static void sigint_handler(int signum);
static void cleanup(void);

int main(void) {
    g_file_rom = file_rom_new(EX_HELLO_WORLD_GUEST);
    if (!g_file_rom) {
        fprintf(stderr, "main: file_rom_new() returned NULL\n");
        cleanup();
        return 1;
    }

    g_print_dev = print_dev_new();
    if (!g_print_dev) {
        fprintf(stderr, "main: print_dev_new() returned NULL\n");
        cleanup();
        return 1;
    }

    g_vm = vm_new();
    vm_err_t err = vm_connect_dev(g_vm, &g_file_rom->desc, g_file_rom);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "main: could not connect file_rom, error %u\n", err);
        cleanup();
        return 1;
    }
    err = vm_connect_dev(g_vm, &g_print_dev->desc, g_print_dev);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "main: could not connect print_dev, error %u\n", err);
        cleanup();
        return 1;
    }

    fprintf(stderr, "main: starting the VM, press Ctrl-C to exit\n");

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "main: failed to install a SIGINT handler: %s\n",
                strerror(errno));
        vm_free(g_vm);
        file_rom_free(g_file_rom);
        print_dev_free(g_print_dev);
        return 1;
    }

    while (g_running) {
        vm_step(g_vm);

        if (g_vm->cpu->num_nested_exc != 0) {
            fprintf(stderr, "main: unexpected exception, shutting down\n");
            break;
        }
    }

    fprintf(stderr, "main: caught signal %u\n", g_caught_signum);
    cleanup();
    return 0;
}

static void sigint_handler(int signum) {
    g_running = false;
    g_caught_signum = signum;
}

void cleanup(void) {
    fprintf(stderr, "cleanup: freeing resources\n");
    if (g_vm) { vm_free(g_vm); }
    if (g_file_rom) { file_rom_free(g_file_rom); }
    if (g_print_dev) { print_dev_free(g_print_dev); }
}
