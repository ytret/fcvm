/**
 * \file cpu_exec.h
 * CPU instruction execution API
 */

#pragma once

#include <fcvm/cpu.h>

vm_err_t cpu_execute_instr(cpu_ctx_t *cpu);
