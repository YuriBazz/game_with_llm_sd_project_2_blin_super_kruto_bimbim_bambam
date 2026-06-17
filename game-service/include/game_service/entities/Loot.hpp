//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_LOOT_HPP
#define GAME_SERVICE_LOOT_HPP

#include <nlohmann/json.hpp>

#include "Item.hpp"

namespace game {

struct Loot {
    int id;
    int x, y;
    Item item;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Loot, id, x, y, item)

}

#endif //GAME_SERVICE_LOOT_HPP