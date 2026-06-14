#include "game_service/RandomGenerator.hpp"

namespace utils {

RandomGenerator::RandomGenerator() : rng(std::random_device{}()) {}

void RandomGenerator::seed(std::uint64_t s) {
    rng.seed(s);
}

int RandomGenerator::get_random(int min, int max) {
    if (min >= max) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}
}
