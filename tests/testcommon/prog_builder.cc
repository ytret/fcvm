#include <string.h>

#include "testcommon/prog_builder.h"

InstrBuilder::InstrBuilder(uint8_t opcode) {
    bytes.push_back(opcode);
}

InstrBuilder &InstrBuilder::reg_code(uint8_t val_reg_code) {
    bytes.push_back(val_reg_code);
    return *this;
}

InstrBuilder &InstrBuilder::imm5(uint8_t val_imm5) {
    return imm8(val_imm5);
}

InstrBuilder &InstrBuilder::imm8(uint8_t val_imm8) {
    bytes.push_back(val_imm8);
    return *this;
}

InstrBuilder &InstrBuilder::imm32(uint32_t val_imm32) {
    bytes.resize(bytes.size() + 4);
    memcpy(bytes.data() + bytes.size() - 4, &val_imm32, 4);
    return *this;
}

ProgBuilder &ProgBuilder::instr(std::vector<uint8_t> instr_bytes) {
    std::copy(instr_bytes.cbegin(), instr_bytes.cend(),
              std::back_inserter(bytes));
    return *this;
}
ProgBuilder &ProgBuilder::instr(const InstrBuilder &instr_builder) {
    return instr(instr_builder.bytes);
}

InstrBuilder build_instr(uint8_t opcode) {
    return InstrBuilder(opcode);
}

ProgBuilder build_prog() {
    return ProgBuilder();
}
