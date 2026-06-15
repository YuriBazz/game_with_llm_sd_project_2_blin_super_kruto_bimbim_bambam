//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_PLAYER_HPP
#define GAME_SERVICE_PLAYER_HPP

#include <nlohmann/json.hpp>
#include <algorithm>

#include "Item.hpp"

namespace game {

struct Player {
    int x, y;
    int hp;
    int max_hp;
    int gold;
    std::vector<Item> inventory;
    int respawns_remaining = 0;
    int spawn_x = 0;
    int spawn_y = 0;
    
    // Вычисленные пассивные бонусы (обновляются при изменении инвентаря)
    int current_damage_bonus = 0;      // Дополнительный урон
    int current_resistance = 0;        // Процент сопротивления (0-66)
    
    // Пересчитать пассивные бонусы из инвентаря
    void recalculate_passives() {
        current_damage_bonus = 0;
        current_resistance = 0;
        
        for (const auto& item : inventory) {
            current_damage_bonus += item.passive_damage_bonus;
            current_resistance += item.passive_resistance_bonus;
        }
        
        // Применить ограничения
        // Максимум 85% от базового урона (базовый = 5, максимум бонус = 4)
        current_damage_bonus = std::min(current_damage_bonus, 4);
        
        // Максимум 66% сопротивления
        current_resistance = std::min(current_resistance, 66);
    }
    
    // Получить итоговый урон с учетом бонусов
    int get_total_damage(int base_damage) const {
        return base_damage + current_damage_bonus;
    }
    
    // Получить урон после сопротивления
    int apply_resistance(int incoming_damage) const {
        int reduced = incoming_damage * (100 - current_resistance) / 100;
        return std::max(1, reduced); // Минимум 1 урон
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Player, x, y, hp, max_hp, gold, inventory,
                                   current_damage_bonus, current_resistance, respawns_remaining,
                                   spawn_x, spawn_y)

}

#endif //GAME_SERVICE_PLAYER_HPP