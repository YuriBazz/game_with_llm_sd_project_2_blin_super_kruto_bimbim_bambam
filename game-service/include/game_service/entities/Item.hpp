//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_ITEM_HPP
#define GAME_SERVICE_ITEM_HPP

#include <nlohmann/json.hpp>

namespace game {

struct Item {
    std::string id;
    std::string name;
    std::string type; // "weapon", "consumable"
    int count = 1;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Item, id, name, type, count)

}

#endif //GAME_SERVICE_ITEM_HPP