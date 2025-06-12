#include <cassert>

#include "testcommon/get_random.h"
#include "testcommon/get_random_prog.h"

std::vector<uint8_t> get_random_instr(std::mt19937 &rng) {
    std::vector<uint8_t> bytes;

    uint8_t opcode;
    const cpu_instr_desc_t *desc;
    do {
        opcode = get_random_opcode(rng);
        desc = cpu_lookup_instr_desc(opcode);
    } while (desc == NULL);

    bytes.push_back(opcode);
    for (size_t opd = 0; opd < desc->num_operands; opd++) {
        switch (desc->operands[opd]) {
        case CPU_OPD_REG:
            bytes.push_back(get_random_reg_code(rng));
            break;
        case CPU_OPD_IMM5:
            bytes.push_back(get_random_imm32(rng) & 31);
            break;
        case CPU_OPD_IMM8:
            bytes.push_back(get_random_imm32(rng) & 0xFF);
            break;
        case CPU_OPD_IMM32:
            bytes.push_back(get_random_imm32(rng));
            break;

        default:
            fprintf(stderr, "%s: operand type %u not impl\n",
                    __PRETTY_FUNCTION__, desc->operands[opd]);
        }
    }

    return bytes;
}

std::vector<uint8_t> get_random_prog(std::mt19937 &rng, size_t min_size) {
    std::vector<uint8_t> prog_bytes;
    while (prog_bytes.size() < min_size) {
        std::vector instr_bytes = get_random_instr(rng);
        std::copy(instr_bytes.begin(), instr_bytes.end(),
                  std::back_inserter(prog_bytes));
    }
    return prog_bytes;
}
