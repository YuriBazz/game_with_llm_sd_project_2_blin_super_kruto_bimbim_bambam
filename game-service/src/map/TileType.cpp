//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/map/TileType.hpp>

namespace game {

// Сериализация TileType как целого числа
void to_json(json& j, const TileType& t) {
    j = static_cast<int>(t);
}

void from_json(const json& j, TileType& t) {
    t = static_cast<TileType>(j.get<int>());
}

const char* tile_type_name(const TileType t) {
    switch (t) {
        case TileType::Floor: return "floor";
        case TileType::Wall:
        default: return "wall";
    }
}

}
