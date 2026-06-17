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
    std::string type; // "weapon", "armor", "consumable"
    int count = 1;
    
    // Пассивные бонусы (применяются автоматически при наличии в инвентаре)
    int passive_damage_bonus = 0;      // Увеличивает урон (макс 85% от базового)
    int passive_resistance_bonus = 0;  // Процент сопротивления урону (макс 66%)
    
    // Активный эффект (для зелий)
    std::string active_effect;  // "heal", "" - нет
    int active_value = 0;       // Значение эффекта (например +20 HP)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Item, id, name, type, count, 
                                   passive_damage_bonus, passive_resistance_bonus,
                                   active_effect, active_value)

}

#endif //GAME_SERVICE_ITEM_HPP