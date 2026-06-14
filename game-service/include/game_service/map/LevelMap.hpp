//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_LEVELMAP_HPP
#define GAME_SERVICE_LEVELMAP_HPP

#include <vector>
#include <nlohmann/json.hpp>

#include "../entities/Enemy.hpp"
#include "Rect.hpp"
#include "TileType.hpp"

namespace game {

struct LevelMap {
    int width;
    int height;
    std::uint64_t seed;

    std::vector<Rect> rooms;
    std::vector<Rect> corridors;

    // Плоский массив для представления 2D сетки (y * width + x)
    std::vector<TileType> grid;

    // Быстрая проверка: может ли игрок встать на эту клетку
    [[nodiscard]] bool is_walkable(int x, int y) const;
};

void to_json(json& j, const LevelMap& map);
void from_json(const json& j, LevelMap& map);

}

#endif //GAME_SERVICE_LEVELMAP_HPP