#include <stdio.h>

#include <ex_common/file_rom.h>
#include <ex_common/print_dev.h>
#include <fcvm/vm.h>

int main(void) {
    const char *guest_prog_path = "hello-world.bin";

    file_rom_ctx_t *file_rom = file_rom_new(guest_prog_path);
    if (!file_rom) {
        fprintf(stderr, "main: file_rom_new() returned NULL\n");
        return 1;
    }

    print_dev_ctx_t *print_dev = print_dev_new();
    if (!print_dev) {
        fprintf(stderr, "main: print_dev_new() returned NULL\n");
        return 1;
    }

    vm_ctx_t *vm = vm_new();
    vm_err_t err = vm_connect_dev(vm, &file_rom->desc, file_rom);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "main: could not connect file_rom, error %u\n", err);
        vm_free(vm);
        return 1;
    }
    err = vm_connect_dev(vm, &print_dev->desc, print_dev);
    if (err != VM_ERR_NONE) {
        fprintf(stderr, "main: could not connect print_dev, error %u\n", err);
        vm_free(vm);
        return 1;
    }

    fprintf(stderr, "main: starting the VM, press Ctrl-C to exit\n");

    while (true) {
        vm_step(vm);

        if (vm->cpu->num_nested_exc != 0) {
            fprintf(stderr, "main: unexpected exception, shutting down\n");
            break;
        }
    }

    vm_free(vm);
    file_rom_free(file_rom);
    return 0;
}
