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

inline constexpr int kPlayerMaxHp = 42;
inline constexpr int kPlayerDamage = 7;
inline constexpr int kDefaultRespawns = 4;
inline constexpr int kPotionHeal = 20;
inline constexpr int kWaveIntervalTicks = 13; // ~5s at 400ms client tick

enum class LevelWinner {
    None,
    Human,
    Ai
};

NLOHMANN_JSON_SERIALIZE_ENUM(LevelWinner, {
    {LevelWinner::None, "none"},
    {LevelWinner::Human, "human"},
    {LevelWinner::Ai, "ai"}
})

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
    Running,
    PlayerVictory,
    EnemyVictory,
    LevelComplete
};

// Конвертация GameState в JSON и обратно
NLOHMANN_JSON_SERIALIZE_ENUM(GameState, {
    {GameState::Running, "running"},
    {GameState::PlayerVictory, "player_victory"},
    {GameState::EnemyVictory, "enemy_victory"},
    {GameState::LevelComplete, "level_complete"}
})

class State {
public:
    LevelMap map;
    Player player;
    Player rival;
    bool session_active = false;
    int session_id = 0;
    bool god_mode = false;
    GameState state;
    int steps{};

    utils::RandomGenerator& rng;

    std::vector<int> entity_grid;
    std::vector<Enemy> enemies;

    int next_loot_id = 0;
    std::vector<Loot> dropped_loot;

    // Campaign / levels
    int campaign_level = 1;
    int player_kills = 0;
    int rival_kills = 0;
    int kills_required = 4;
    int max_enemies_on_level = 28;
    int enemy_activation_radius = 8;
    int level_loot_bonus = 0;
    int level_enemy_hp_bonus = 0;
    int next_enemy_id = 0;
    int wave_tick_counter = 0;
    int player_ticks_since_death = 0;
    int rival_ticks_since_death = 0;
    LevelWinner level_winner = LevelWinner::None;
    static constexpr int kRespawnDelayTicks = 5;

    // Real-time параметры
    std::chrono::milliseconds turn_duration{500};  // Длительность одного хода в мс
    int action_duration_ms = 200;                  // Время разрешения действия (анимация)
    int player_last_action_time = 0;               // Последнее время действия игрока (миллисекунды)
    std::vector<int> enemy_last_action_times;      // Последнее время действия каждого врага (миллисекунды)

    State(utils::RandomGenerator& rng);

    // Инициализация новой игры
    void start_new_game(const MapOptions &options);
    void reset_campaign();
    void advance_campaign_level();
    void apply_level_scaling();

    // slot: "player" (default, human) or "rival" (agent)
    MoveResult move_slot(const std::string& slot, int dx, int dy);
    AttackResult attack_slot(const std::string& slot, int dx, int dy);
    UseItemResult use_item_slot(const std::string& slot, const std::string& item_id);

    MoveResult move(int dx, int dy) { return move_slot("player", dx, dy); }
    AttackResult attack(int dx, int dy) { return attack_slot("player", dx, dy); }
    UseItemResult use_item(const std::string& item_id) { return use_item_slot("player", item_id); }

    // Автоматический ход врагов после действия игрока
    void process_enemy_turns();
    void process_realtime_tick();
    void try_respawn_players();
    void spawn_wave();

    // Управление врагами через API
    MoveResult enemy_move(int enemy_index, int dx, int dy);
    AttackResult enemy_attack(int enemy_index, int dx, int dy);

    // Real-time обновления
    void update(int current_time_ms);
    void check_victory_conditions();

    // Проверка готовности юнитов к действиям
    [[nodiscard]] bool is_player_action_resolved(int current_time_ms) const;
    [[nodiscard]] bool is_enemy_action_resolved(int enemy_index, int current_time_ms) const;

    [[nodiscard]] json get_full_map_cells() const;
    [[nodiscard]] bool player_blocks_world() const;
    [[nodiscard]] json get_visible_cells(int radius, const std::string& slot = "player") const;
    [[nodiscard]] json get_visible_cells_for(int cx, int cy, int radius) const;
    [[nodiscard]] json scout_around(const std::string& slot = "rival",
                                    int entry_corridor_hint = -1,
                                    const std::vector<int>& blocked_corridors = {}) const;
    [[nodiscard]] std::vector<std::string> get_available_actions(const std::string& slot = "player") const;
    [[nodiscard]] json get_enemy_visible_cells(int enemy_index, int radius) const;
    [[nodiscard]] std::vector<json> get_enemies_state() const;
    [[nodiscard]] int room_index_at(int x, int y) const;

private:
    // Внутренний метод перемещения врага с обновлением entity_grid
    void move_enemy(size_t enemy_index, int new_x, int new_y);

    [[nodiscard]] bool has_enemy_at(int x, int y) const;

    [[nodiscard]] bool is_cell_blocked(int x, int y, int ignore_x = -1, int ignore_y = -1) const;

    MoveResult move_actor(Player& actor, int dx, int dy, int ignore_x, int ignore_y);
    AttackResult attack_actor(Player& actor, int dx, int dy);
    PickupResult collect_loot_at(Player& actor);
    UseItemResult use_item_actor(Player& actor, const std::string& item_id);

    [[nodiscard]] bool is_enemy_active(const Enemy& enemy) const;
    [[nodiscard]] std::pair<int, int> bfs_next_step(int from_x, int from_y, int to_x, int to_y) const;

    [[nodiscard]] Player* actor_for_slot(const std::string& slot);
    [[nodiscard]] const Player* actor_for_slot(const std::string& slot) const;
    [[nodiscard]] bool is_rival_actor(const Player& actor) const;

    [[nodiscard]] bool corridor_links_room(size_t corridor_index, int room_index) const;
    [[nodiscard]] int corridor_destination_room(size_t corridor_index, int from_room_index) const;
    [[nodiscard]] int corridor_index_at(int x, int y) const;
    [[nodiscard]] int entry_corridor_for_room(int room_index, int ax, int ay) const;
    [[nodiscard]] std::vector<std::pair<int, int>> bfs_path_cells(
        int from_x, int from_y, int to_x, int to_y, bool block_actors = true) const;
    [[nodiscard]] std::vector<std::string> cells_to_directions(
        int from_x, int from_y, const std::vector<std::pair<int, int>>& cells) const;

    void credit_kill(const Player& killer);
    void drop_scaled_loot(int x, int y);
    void spawn_enemies(int count);
    void spawn_players_at_opposite_corners();
    void place_player_at_room(Player& actor, size_t room_index, bool near_start_corner);
    [[nodiscard]] MapOptions scaled_map_options(const MapOptions& base) const;

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