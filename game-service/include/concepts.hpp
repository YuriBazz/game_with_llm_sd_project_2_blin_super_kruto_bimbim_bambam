//
// Created by Richard Dzubko on 14.06.2026.
//

#ifndef GAME_SERVICE_CONCEPTS_HPP
#define GAME_SERVICE_CONCEPTS_HPP
#include <type_traits>

namespace utils {
template<typename T> concept IsEnum = std::is_enum_v<T>;
}

#endif //GAME_SERVICE_CONCEPTS_HPP