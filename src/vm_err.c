#pragma once

#include "debugm.h"
#include "vm_err.h"

uint8_t vm_err_to_irq_line(vm_err_t err) {
    D_ASSERTM(err.type != VM_ERR_NONE,
              "VM_ERR_NONE has no corresponding IRQ line");
    D_ASSERTMF((uint32_t)err.type < UINT8_MAX,
               "value %u does not fit into uint8_t", (uint32_t)err.type);
    return (uint8_t)err.type - 1;
}
