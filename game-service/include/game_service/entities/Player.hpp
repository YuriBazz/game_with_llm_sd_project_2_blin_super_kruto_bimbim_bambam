//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_PLAYER_HPP
#define GAME_SERVICE_PLAYER_HPP

#include <nlohmann/json.hpp>

#include "Item.hpp"

namespace game {

struct Player {
    int x, y;
    int hp;
    int max_hp;
    int gold;
    std::vector<Item> inventory;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Player, x, y, hp, max_hp, gold, inventory)

}

#endif //GAME_SERVICE_PLAYER_HPP