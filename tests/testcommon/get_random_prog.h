#pragma once

#include <cstdint>
#include <random>
#include <vector>

std::vector<uint8_t> get_random_instr(std::mt19937 &rng);
std::vector<uint8_t> get_random_prog(std::mt19937 &rng, size_t min_size);
