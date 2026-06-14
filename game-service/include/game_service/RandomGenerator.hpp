//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_RANDOMGENERATOR_HPP
#define GAME_SERVICE_RANDOMGENERATOR_HPP

#include <random>

#include "concepts.hpp"
#include "magic_enum.hpp"

namespace utils {

class RandomGenerator {
    std::mt19937 rng;

public:
    RandomGenerator();

    void seed(std::uint64_t s);

    int get_random(int min, int max);

    template<IsEnum E> E get_random_enum() {
        const auto count = magic_enum::enum_count<E>();
        static_assert(count > 0, "Enum type must have at least one value");

        int index = get_random(0, count - 1);
        return static_cast<E>(index);
    }
};

}

#endif //GAME_SERVICE_RANDOMGENERATOR_HPP