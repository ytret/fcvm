#include <iostream>
#include <vector>

#include <cpu.h>

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

    return 0;
}
