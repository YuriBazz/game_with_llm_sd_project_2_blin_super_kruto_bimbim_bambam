//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/map/LevelMap.hpp>

namespace game {

// Проверка проходимости
bool LevelMap::is_walkable(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return false;
    }
    return grid[y * width + x] == TileType::Floor;
}

// Сериализация готовой карты
void to_json(json& j, const LevelMap& map) {
    j = json{
            {"width", map.width},
            {"height", map.height},
            {"seed", map.seed},
            {"rooms", map.rooms},
            {"corridors", map.corridors},
            {"grid", map.grid}
    };
}

void from_json(const json& j, LevelMap& map) {
    map.width = j.value("width", 0);
    map.height = j.value("height", 0);
    map.seed = j.value("seed", 0);

    if (j.contains("rooms")) j.at("rooms").get_to(map.rooms);
    if (j.contains("corridors")) j.at("corridors").get_to(map.corridors);
    if (j.contains("grid")) j.at("grid").get_to(map.grid);
}

}