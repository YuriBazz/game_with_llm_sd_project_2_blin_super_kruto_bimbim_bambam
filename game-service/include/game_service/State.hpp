//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_STATE_HPP
#define GAME_SERVICE_STATE_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "RandomGenerator.hpp"
#include "entities/Item.hpp"
#include "entities/Loot.hpp"
#include "entities/Player.hpp"
#include "map/LevelMap.hpp"
#include "map/MapOptions.hpp"

namespace game {

inline constexpr int kPlayerMaxHp = 30;
inline constexpr int kPlayerDamage = 5;
inline constexpr int kPotionHeal = 20;

struct MoveResult {
    bool success = false;
    int x = 0;
    int y = 0;
};

struct AttackResult {
    bool success = false;
    int damage = 0;
    bool target_dead = false;
};

struct PickupResult {
    bool success = false;
    std::vector<Item> items;
};

struct UseItemResult {
    bool success = false;
    std::string effect;
    int value = 0;
};

enum class Phase {
    PlayerTurn,  // Ожидание ввода от игрока
    EnemyTurn,   // Просчет ходов ИИ
    PlayerDead,  // Экран Game Over
    Victory      // Игрок нашел выход
};

// Конвертация GamePhase в JSON и обратно
NLOHMANN_JSON_SERIALIZE_ENUM(Phase, {
    {Phase::PlayerTurn, "player_turn"},
    {Phase::EnemyTurn, "enemy_turn"},
    {Phase::PlayerDead, "player_dead"},
    {Phase::Victory, "victory"}
})

class State {
public:
    LevelMap map;
    Player player;
    Phase phase;
    int steps;

    utils::RandomGenerator& rng;

    std::vector<int> entity_grid;
    std::vector<Enemy> enemies;

    int next_loot_id = 0;
    std::vector<Loot> dropped_loot;

    State(utils::RandomGenerator& rng);

    // Инициализация новой игры
    void start_new_game(MapOptions options);

    MoveResult move(int dx, int dy);
    AttackResult attack(int dx, int dy);
    PickupResult pickup();
    UseItemResult use_item(const std::string& item_id);

    [[nodiscard]] json get_visible_cells(int radius) const;
    [[nodiscard]] std::vector<std::string> get_available_actions() const;

private:
    // Логика хода всех врагов
    void process_enemy_turn();

    void end_player_turn(); // Вспомогательный метод для завершения хода

    // Внутренний метод перемещения врага с обновлением entity_grid
    void move_enemy(size_t enemy_index, int new_x, int new_y);

    [[nodiscard]] bool has_enemy_at(int x, int y) const;

    bool can_player_move_to(int target_x, int target_y);

    [[nodiscard]] Enemy* get_enemy_at(int x, int y);

    // Вспомогательная функция для генерации лута
    void drop_loot(int x, int y, const Item& item);
};

// Поддержка сохранения/загрузки всего состояния игры в JSON
void to_json(json& j, const State& state);
void from_json(const json& j, State& state);

}

#endif //GAME_SERVICE_STATE_HPP