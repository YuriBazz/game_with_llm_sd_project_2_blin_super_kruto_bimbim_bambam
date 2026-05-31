//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_TILETYPE_HPP
#define GAME_SERVICE_TILETYPE_HPP

#include <nlohmann/json.hpp>

namespace game {

using json = nlohmann::json;

enum class TileType {
    Wall = 0,
    Floor = 1
};

void to_json(json& j, const TileType& t);
void from_json(const json& j, TileType& t);

[[nodiscard]] const char* tile_type_name(TileType t);

}

#endif //GAME_SERVICE_TILETYPE_HPP