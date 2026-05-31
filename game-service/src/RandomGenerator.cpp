#include "game_service/RandomGenerator.hpp"

namespace utils {

RandomGenerator::RandomGenerator() : rng(std::random_device{}()) {}

}
