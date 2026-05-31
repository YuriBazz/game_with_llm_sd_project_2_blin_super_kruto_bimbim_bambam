//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_RANDOMGENERATOR_HPP
#define GAME_SERVICE_RANDOMGENERATOR_HPP

#include <random>

namespace utils {

class RandomGenerator {
    std::mt19937 rng;

public:
    RandomGenerator();

    void seed(std::uint64_t s) {
        rng.seed(s);
    }

    int get_random(int min, int max) {
        if (min >= max) return min;
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }
};

}

#endif //GAME_SERVICE_RANDOMGENERATOR_HPP