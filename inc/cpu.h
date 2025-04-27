#pragma once

#include "vm.h"

#define CPU_OP_MOV_RR   0x10
#define CPU_OP_MOV_VR   0x11
#define CPU_OP_STR_RI0  0x12
#define CPU_OP_STR_RV0  0x13
#define CPU_OP_STR_RI8  0x14
#define CPU_OP_STR_RI32 0x15
#define CPU_OP_STR_RIR  0x16

#define CPU_OP_LDR_RV0  0x17
#define CPU_OP_LDR_RI0  0x18
#define CPU_OP_LDR_RI8  0x19
#define CPU_OP_LDR_RI32 0x1A
#define CPU_OP_LDR_RIR  0x1B

static_assert(VM_NUM_GP_REGS == 8, "please update register codec");
#define CPU_CODE_R0 0x00
#define CPU_CODE_R1 0x01
#define CPU_CODE_R2 0x02
#define CPU_CODE_R3 0x03
#define CPU_CODE_R4 0x04
#define CPU_CODE_R5 0x05
#define CPU_CODE_R6 0x06
#define CPU_CODE_R7 0x07
#define CPU_CODE_SP 0x20

void cpu_init(vm_state_t *vm);
void cpu_deinit(vm_state_t *vm);

void cpu_reset(vm_state_t *vm);
void cpu_step(vm_state_t *vm);
