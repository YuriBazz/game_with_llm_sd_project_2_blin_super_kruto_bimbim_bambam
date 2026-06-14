//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_STATE_HPP
#define GAME_SERVICE_STATE_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>

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

enum class GameState {
    Running,          // Игра в процессе
    PlayerVictory,    // Все враги убиты
    EnemyVictory      // Герой убит
};

// Конвертация GameState в JSON и обратно
NLOHMANN_JSON_SERIALIZE_ENUM(GameState, {
    {GameState::Running, "running"},
    {GameState::PlayerVictory, "player_victory"},
    {GameState::EnemyVictory, "enemy_victory"}
})

class State {
public:
    LevelMap map;
    Player player;
    GameState state;
    int steps{};

    utils::RandomGenerator& rng;

    std::vector<int> entity_grid;
    std::vector<Enemy> enemies;

    int next_loot_id = 0;
    std::vector<Loot> dropped_loot;

    // Real-time параметры
    std::chrono::milliseconds turn_duration{500};  // Длительность одного хода в мс
    int action_duration_ms = 200;                  // Время разрешения действия (анимация)
    int player_last_action_time = 0;               // Последнее время действия игрока (миллисекунды)
    std::vector<int> enemy_last_action_times;      // Последнее время действия каждого врага (миллисекунды)

    State(utils::RandomGenerator& rng);

    // Инициализация новой игры
    void start_new_game(const MapOptions &options);

    // Управление игроком
    MoveResult move(int dx, int dy);
    AttackResult attack(int dx, int dy);
    PickupResult pickup();
    UseItemResult use_item(const std::string& item_id);

    // Управление врагами через API
    MoveResult enemy_move(int enemy_index, int dx, int dy);
    AttackResult enemy_attack(int enemy_index, int dx, int dy);

    // Real-time обновления
    void update(int current_time_ms);
    void check_victory_conditions();

    // Проверка готовности юнитов к действиям
    [[nodiscard]] bool is_player_action_resolved(int current_time_ms) const;
    [[nodiscard]] bool is_enemy_action_resolved(int enemy_index, int current_time_ms) const;

    [[nodiscard]] json get_visible_cells(int radius) const;
    [[nodiscard]] json get_enemy_visible_cells(int enemy_index, int radius) const;
    [[nodiscard]] std::vector<std::string> get_available_actions() const;
    [[nodiscard]] std::vector<json> get_enemies_state() const;

private:
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