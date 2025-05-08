#pragma once

#include <vector>

#include <stdint.h>

class InstrBuilder {
  public:
    InstrBuilder(uint8_t opcode);
    InstrBuilder &reg_code(uint8_t val_reg_code);
    InstrBuilder &imm5(uint8_t val_imm5);
    InstrBuilder &imm8(uint8_t val_imm8);
    InstrBuilder &imm32(uint32_t val_imm32);

    std::vector<uint8_t> bytes;
};

class ProgBuilder {
  public:
    ProgBuilder &instr(std::vector<uint8_t> instr_bytes);
    ProgBuilder &instr(const InstrBuilder &instr_builder);

    std::vector<uint8_t> bytes;
};

InstrBuilder build_instr(uint8_t opcode);
ProgBuilder build_prog();
