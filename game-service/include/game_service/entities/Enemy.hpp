//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_ENEMY_HPP
#define GAME_SERVICE_ENEMY_HPP

#include <string>
#include <nlohmann/json.hpp>

enum class EnemyType {
    Goblin,
    Orc,
    Troll
};

NLOHMANN_JSON_SERIALIZE_ENUM(EnemyType, {
    {EnemyType::Goblin, "goblin"},
    {EnemyType::Orc, "orc"},
    {EnemyType::Troll, "troll"},
})

struct Enemy {
    int id;
    int x, y;
    int hp;
    EnemyType type;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Enemy, id, x, y, hp, type)

#endif //GAME_SERVICE_ENEMY_HPP