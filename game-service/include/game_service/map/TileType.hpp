//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_TILETYPE_HPP
#define GAME_SERVICE_TILETYPE_HPP

#include <nlohmann/json.hpp>
#include <game_service/magic_enum.hpp>

namespace game {

using json = nlohmann::json;

enum class TileType {
    Wall,
    Floor
};

void to_json(json& j, const TileType& t);
void from_json(const json& j, TileType& t);

template <TileType> consteval std::string_view name_of();

template <> consteval std::string_view name_of<TileType::Wall>() {
    return "wall";
}

template <> consteval std::string_view name_of<TileType::Floor>() {
    return "floor";
}

template<std::size_t... I>
consteval auto make_table(std::index_sequence<I...>) {
    return std::array<std::string_view, sizeof...(I)>{
        name_of<static_cast<TileType>(I)>()...
    };
}

constexpr auto tile_names = make_table(std::make_index_sequence<magic_enum::enum_count<TileType>()>{});

constexpr std::string_view tile_type_name(TileType t) {
    return tile_names[static_cast<std::size_t>(t)];
}

}

#endif//GAME_SERVICE_TILETYPE_HPP
