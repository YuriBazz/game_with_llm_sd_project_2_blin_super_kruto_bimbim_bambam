//
// Created by Richard Dzubko on 31.05.2026.
//

#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <utility>

#include <game_service/State.hpp>
#include "game_service/map/MapGenerator.hpp"
#include "game_service/map/TileType.hpp"

namespace game {

namespace {

std::string game_state_to_phase(GameState state) {
    switch (state) {
        case GameState::Running:
            return "realtime";
        case GameState::PlayerVictory:
            return "victory";
        case GameState::EnemyVictory:
            return "player_dead";
        case GameState::LevelComplete:
            return "level_complete";
    }
    return "player_turn";
}

bool is_game_over(GameState state) {
    return state != GameState::Running;
}

bool is_won(GameState state) {
    return state == GameState::PlayerVictory;
}

bool is_level_complete(GameState state) {
    return state == GameState::LevelComplete;
}

struct EnemyTypeStats {
    int hp;
    int damage;
};

EnemyTypeStats enemy_stats_for(EnemyType type) {
    switch (type) {
        case EnemyType::Goblin:
            return {5, 1};
        case EnemyType::Orc:
            return {10, 2};
        case EnemyType::Troll:
            return {18, 3};
        case EnemyType::Rat:
            return {3, 1};
    }
    return {6, 1};
}

int scaled_potion_heal(int level_loot_bonus) {
    return game::kPotionHeal + level_loot_bonus;
}

int scaled_sword_bonus(int level_loot_bonus) {
    return 2 + level_loot_bonus / 2;
}

int scaled_armor_resist(int level_loot_bonus) {
    return 20 + level_loot_bonus;
}

} // namespace

void State::apply_level_scaling() {
    kills_required = 4 + (campaign_level - 1) * 5;
    enemy_activation_radius = 6 + (campaign_level - 1);
    level_loot_bonus = (campaign_level - 1) * 2;
    level_enemy_hp_bonus = (campaign_level - 1) * 2;
    max_enemies_on_level = 10 + campaign_level * 4;
}

MapOptions State::scaled_map_options(const MapOptions& base) const {
    MapOptions scaled = base;
    const int growth = (campaign_level - 1) * 10;
    scaled.map_width = std::max(base.map_width, 40) + growth;
    scaled.map_height = std::max(base.map_height, 40) + growth;
    return scaled;
}

void State::reset_campaign() {
    campaign_level = 1;
    apply_level_scaling();
}

void State::advance_campaign_level() {
    ++campaign_level;
    apply_level_scaling();
}

bool State::player_blocks_world() const {
    return player.hp > 0;
}

Player* State::actor_for_slot(const std::string& slot) {
    if (slot == "rival") return &rival;
    if (slot == "player") return &player;
    return nullptr;
}

const Player* State::actor_for_slot(const std::string& slot) const {
    if (slot == "rival") return &rival;
    if (slot == "player") return &player;
    return nullptr;
}

bool State::is_rival_actor(const Player& actor) const {
    return &actor == &rival;
}

bool State::is_enemy_active(const Enemy& enemy) const {
    if (enemy.type == EnemyType::Rat) return true;

    const int radius_sq = enemy_activation_radius * enemy_activation_radius;
    auto within = [&](int px, int py, int hp) {
        if (hp <= 0) return false;
        const int dx = enemy.x - px;
        const int dy = enemy.y - py;
        return dx * dx + dy * dy <= radius_sq;
    };
    if (within(rival.x, rival.y, rival.hp)) return true;
    if (within(player.x, player.y, player.hp)) return true;
    return false;
}

std::pair<int, int> State::bfs_next_step(int from_x, int from_y, int to_x, int to_y) const {
    if (from_x == to_x && from_y == to_y) return {0, 0};

    const int width = map.width;
    const int height = map.height;
    const auto index = [width](int x, int y) { return y * width + x; };
    const int start = index(from_x, from_y);
    const int goal = index(to_x, to_y);

    std::vector<int> parent(width * height, -1);
    std::queue<std::pair<int, int>> q;
    q.push({from_x, from_y});
    parent[start] = start;

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    auto walkable = [&](int x, int y) {
        if (!map.is_walkable(x, y)) return false;
        if (has_enemy_at(x, y)) return false;
        if (player_blocks_world() && player.x == x && player.y == y) return false;
        if (rival.hp > 0 && rival.x == x && rival.y == y) return false;
        return true;
    };

    while (!q.empty()) {
        const auto [cx, cy] = q.front();
        q.pop();
        const int ci = index(cx, cy);

        if (ci == goal) {
            int cur = goal;
            while (parent[cur] != cur && parent[cur] != start) {
                cur = parent[cur];
            }
            if (parent[cur] == start) {
                const int fx = cur % width;
                const int fy = cur / width;
                return {fx - from_x, fy - from_y};
            }
            return {0, 0};
        }

        for (const auto& dir : dirs) {
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

            const int ni = index(nx, ny);
            if (parent[ni] != -1) continue;
            if (!walkable(nx, ny) && ni != goal) continue;

            parent[ni] = ci;
            q.push({nx, ny});
        }
    }

    return {0, 0};
}

std::vector<std::pair<int, int>> State::reachable_cells_from(int from_x, int from_y) const {
    std::vector<std::pair<int, int>> cells;
    if (from_x < 0 || from_y < 0 || from_x >= map.width || from_y >= map.height) {
        return cells;
    }

    const int width = map.width;
    const int height = map.height;
    const auto index = [width](int x, int y) { return y * width + x; };
    const int start = index(from_x, from_y);

    std::vector<bool> visited(width * height, false);
    std::queue<std::pair<int, int>> q;
    q.push({from_x, from_y});
    visited[start] = true;
    cells.push_back({from_x, from_y});

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    auto walkable = [&](int x, int y) {
        if (!map.is_walkable(x, y)) return false;
        if (has_enemy_at(x, y)) return false;
        if (player_blocks_world() && player.x == x && player.y == y) return false;
        if (rival.hp > 0 && rival.x == x && rival.y == y) return false;
        return true;
    };

    while (!q.empty()) {
        const auto [cx, cy] = q.front();
        q.pop();

        for (const auto& dir : dirs) {
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

            const int ni = index(nx, ny);
            if (visited[ni]) continue;
            if (!walkable(nx, ny)) continue;

            visited[ni] = true;
            cells.push_back({nx, ny});
            q.push({nx, ny});
        }
    }

    return cells;
}

void State::process_rat_turn(int enemy_index) {
    if (enemy_index < 0 || enemy_index >= static_cast<int>(enemies.size())) return;

    Enemy& enemy = enemies[enemy_index];
    const bool at_target = enemy.target_x >= 0 &&
                           enemy.x == enemy.target_x &&
                           enemy.y == enemy.target_y;

    if (enemy.target_x < 0 || at_target) {
        const auto reachable = reachable_cells_from(enemy.x, enemy.y);
        if (reachable.empty()) return;

        std::vector<std::pair<int, int>> candidates;
        candidates.reserve(reachable.size());
        for (const auto& cell : reachable) {
            if (cell.first == enemy.x && cell.second == enemy.y) continue;
            if (cell.first == enemy.target_x && cell.second == enemy.target_y) continue;
            candidates.push_back(cell);
        }
        if (candidates.empty()) {
            enemy.target_x = -1;
            enemy.target_y = -1;
            return;
        }

        const size_t pick = static_cast<size_t>(rng.get_random(0, static_cast<int>(candidates.size()) - 1));
        enemy.target_x = candidates[pick].first;
        enemy.target_y = candidates[pick].second;
    }

    const auto step = bfs_next_step(enemy.x, enemy.y, enemy.target_x, enemy.target_y);
    if (step.first != 0 || step.second != 0) {
        enemy_move(enemy_index, step.first, step.second);
        return;
    }

    // Blocked or stuck — pick a different wander target next turn.
    enemy.target_x = -1;
    enemy.target_y = -1;
}

void State::credit_kill(const Player& killer) {
    if (is_rival_actor(killer)) {
        ++rival_kills;
    } else {
        ++player_kills;
    }
    check_victory_conditions();
}

void State::drop_scaled_loot(int x, int y) {
    const int chance = rng.get_random(1, 100);

    if (chance <= 35) {
        Item potion{"potion", "Health Potion", "consumable", 1, 0, 0, "heal",
                    scaled_potion_heal(level_loot_bonus)};
        drop_loot(x, y, potion);
    } else if (chance <= 45) {
        Item extra_life{"extra_life", "Extra Life", "consumable", 1, 0, 0, "respawn", 1};
        drop_loot(x, y, extra_life);
    } else if (chance <= 55) {
        Item sword{"sword", "Iron Sword", "weapon", 1, scaled_sword_bonus(level_loot_bonus), 0, "", 0};
        drop_loot(x, y, sword);
    } else if (chance <= 90) {
        Item armor{"armor", "Steel Armor", "armor", 1, 0, scaled_armor_resist(level_loot_bonus), "", 0};
        drop_loot(x, y, armor);
    }
}

void State::spawn_wave() {
    if (state != GameState::Running) return;
    if (static_cast<int>(enemies.size()) >= max_enemies_on_level) return;

    const int want = rng.get_random(5, 6);
    const int can_spawn = max_enemies_on_level - static_cast<int>(enemies.size());
    spawn_enemies(std::min(want, can_spawn));
}

void State::spawn_enemies(int count) {
    if (map.rooms.empty() || count <= 0) return;

    int spawned = 0;
    int attempts = 0;
    const int max_attempts = count * 30;

    while (spawned < count && attempts < max_attempts) {
        ++attempts;
        const int room_idx = rng.get_random(0, static_cast<int>(map.rooms.size()) - 1);
        const auto& room = map.rooms[static_cast<size_t>(room_idx)];

        const int enemy_x = room.x + rng.get_random(0, room.w - 1);
        const int enemy_y = room.y + rng.get_random(0, room.h - 1);
        const int idx = enemy_y * map.width + enemy_x;

        if (map.grid[idx] != TileType::Floor || entity_grid[idx] != -1) continue;
        if (enemy_x == player.x && enemy_y == player.y) continue;
        if (enemy_x == rival.x && enemy_y == rival.y) continue;
        if (std::abs(enemy_x - player.x) + std::abs(enemy_y - player.y) < 2) continue;
        if (std::abs(enemy_x - rival.x) + std::abs(enemy_y - rival.y) < 2) continue;

        const EnemyType type = rng.get_random_enum<EnemyType>();
        const EnemyTypeStats stats = enemy_stats_for(type);
        Enemy enemy{next_enemy_id++, enemy_x, enemy_y, stats.hp + level_enemy_hp_bonus, type};

        enemies.push_back(enemy);
        entity_grid[idx] = enemy.id;
        enemy_last_action_times.push_back(0);
        ++spawned;
    }
}

bool State::has_enemy_at(int x, int y) const {
    if (x < 0 || x >= map.width || y < 0 || y >= map.height) return false;
    return entity_grid[y * map.width + x] != -1;
}

bool State::can_player_move_to(int target_x, int target_y) {
    // 1. Проверяем геометрию карты (стены)
    if (!map.is_walkable(target_x, target_y)) {
        return false;
    }

    // 2. Проверяем существ
    if (has_enemy_at(target_x, target_y)) {
        return false;
    }

    return true;
}

Enemy * State::get_enemy_at(int x, int y) {
    if (!has_enemy_at(x, y)) return nullptr;
    const int id = entity_grid[y * map.width + x];

    // Ищем врага по ID
    for (auto& enemy : enemies) {
        if (enemy.id == id) return &enemy;
    }
    return nullptr;
}

void State::drop_loot(int x, int y, const Item &item) {
    dropped_loot.push_back({next_loot_id++, x, y, item});
}

State::State(utils::RandomGenerator& rng) : map(), player{0, 0, 0, 0, 0}, rival{0, 0, 0, 0, 0}, state(GameState::Running), rng{rng} {
    apply_level_scaling();
}

void State::start_new_game(const MapOptions &options) {
    // Очищаем состояние для новой игры
    enemies.clear();
    dropped_loot.clear();
    player.inventory.clear();
    player.current_damage_bonus = 0;
    player.current_resistance = 0;
    next_loot_id = 0;
    next_enemy_id = 0;
    player_kills = 0;
    rival_kills = 0;
    level_winner = LevelWinner::None;
    wave_tick_counter = 0;
    player_ticks_since_death = 0;
    rival_ticks_since_death = 0;
    steps = 0;
    state = GameState::Running;
    session_active = false;
    player_last_action_time = 0;
    enemy_last_action_times.clear();

    MapGenerator generator(rng);
    map = generator.generate_map(scaled_map_options(options));

    entity_grid.assign(map.width * map.height, -1);

    if (!map.rooms.empty()) {
        spawn_players_at_opposite_corners();
    }

    spawn_enemies(4 + campaign_level * 2);
    spectator.x = map.width / 2;
    spectator.y = map.height / 2;
    ++session_id;
    session_active = true;
}

void State::place_player_at_room(Player& actor, size_t room_index, bool near_start_corner) {
    if (map.rooms.empty() || room_index >= map.rooms.size()) return;

    const auto& room = map.rooms[room_index];
    const int base_x = near_start_corner ? room.x + 1 : room.x + std::max(1, room.w - 2);
    const int base_y = near_start_corner ? room.y + 1 : room.y + std::max(1, room.h - 2);

    static constexpr int dirs[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& dir : dirs) {
        const int tx = base_x + dir[0];
        const int ty = base_y + dir[1];
        if (!map.is_walkable(tx, ty)) continue;
        if (has_enemy_at(tx, ty)) continue;
        actor.x = tx;
        actor.y = ty;
        actor.spawn_x = tx;
        actor.spawn_y = ty;
        return;
    }

    actor.x = room.x + room.w / 2;
    actor.y = room.y + room.h / 2;
    actor.spawn_x = actor.x;
    actor.spawn_y = actor.y;
}

void State::spawn_players_at_opposite_corners() {
    player = Player{};
    player.max_hp = kPlayerMaxHp + (campaign_level - 1) * 3;
    player.hp = player.max_hp;
    player.gold = 0;
    player.respawns_remaining = kDefaultRespawns;
    place_player_at_room(player, 0, true);
    player.recalculate_passives();

    rival = Player{};
    rival.max_hp = kPlayerMaxHp + (campaign_level - 1) * 3;
    rival.hp = rival.max_hp;
    rival.gold = 0;
    rival.respawns_remaining = kDefaultRespawns;
    const size_t rival_room = map.rooms.size() > 1 ? map.rooms.size() - 1 : 0;
    place_player_at_room(rival, rival_room, false);
    rival.recalculate_passives();

    Item potion{"potion", "Health Potion", "consumable", 1, 0, 0, "heal",
                scaled_potion_heal(level_loot_bonus)};
    rival.inventory.push_back(potion);
}

void State::try_respawn_players() {
    if (state != GameState::Running) return;

    auto try_one = [&](Player& actor, int& ticks_since_death) {
        if (actor.hp > 0) {
            ticks_since_death = 0;
            return;
        }
        if (actor.respawns_remaining <= 0) return;

        ++ticks_since_death;
        if (ticks_since_death < kRespawnDelayTicks) return;

        actor.respawns_remaining--;
        actor.hp = actor.max_hp;
        actor.inventory.clear();
        actor.current_damage_bonus = 0;
        actor.current_resistance = 0;
        actor.recalculate_passives();
        ticks_since_death = 0;

        actor.x = actor.spawn_x;
        actor.y = actor.spawn_y;
        if (!map.is_walkable(actor.x, actor.y) || has_enemy_at(actor.x, actor.y)) {
            static constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& dir : dirs) {
                const int tx = actor.spawn_x + dir[0];
                const int ty = actor.spawn_y + dir[1];
                if (map.is_walkable(tx, ty) && !has_enemy_at(tx, ty)) {
                    actor.x = tx;
                    actor.y = ty;
                    break;
                }
            }
        }
    };

    try_one(player, player_ticks_since_death);
    try_one(rival, rival_ticks_since_death);
}

void State::process_realtime_tick() {
    if (state != GameState::Running || !session_active) return;

    try_respawn_players();

    ++wave_tick_counter;
    if (wave_tick_counter >= kWaveIntervalTicks) {
        wave_tick_counter = 0;
        spawn_wave();
    }
}

void State::move_enemy(size_t enemy_index, int new_x, int new_y) {
    if (enemy_index >= enemies.size()) return;
    
    Enemy& enemy = enemies[enemy_index];

    // Освобождаем старую клетку в сетке
    entity_grid[enemy.y * map.width + enemy.x] = -1;

    // Обновляем координаты
    enemy.x = new_x;
    enemy.y = new_y;

    // Занимаем новую клетку
    entity_grid[new_y * map.width + new_x] = enemy.id;
}

bool State::is_cell_blocked(int x, int y, int ignore_x, int ignore_y) const {
    if (!map.is_walkable(x, y)) return true;
    if (has_enemy_at(x, y)) return true;
    if (player_blocks_world() && player.x == x && player.y == y &&
        !(x == ignore_x && y == ignore_y)) return true;
    if (rival.hp > 0 && rival.x == x && rival.y == y &&
        !(x == ignore_x && y == ignore_y)) return true;
    return false;
}

MoveResult State::move_actor(Player& actor, int dx, int dy, int ignore_x, int ignore_y) {
    MoveResult result;
    if (state != GameState::Running || actor.hp <= 0) return result;

    const int target_x = actor.x + dx;
    const int target_y = actor.y + dy;

    if (is_cell_blocked(target_x, target_y, ignore_x, ignore_y)) {
        return result;
    }

    actor.x = target_x;
    actor.y = target_y;
    result.success = true;
    result.x = actor.x;
    result.y = actor.y;

    collect_loot_at(actor);

    player_last_action_time = steps;
    steps++;
    return result;
}

AttackResult State::attack_actor(Player& actor, int dx, int dy) {
    AttackResult result;
    if (state != GameState::Running || actor.hp <= 0) return result;

    const int target_x = actor.x + dx;
    const int target_y = actor.y + dy;
    const int base_damage = kPlayerDamage;

    Enemy* target = get_enemy_at(target_x, target_y);
    if (!target) return result;

    result.damage = actor.get_total_damage(base_damage);
    result.success = true;
    target->hp -= result.damage;

    if (target->hp <= 0) {
        result.target_dead = true;
        drop_scaled_loot(target->x, target->y);

        entity_grid[target->y * map.width + target->x] = -1;

        for (size_t i = 0; i < enemies.size(); ++i) {
            if (enemies[i].id == target->id) {
                enemies.erase(enemies.begin() + i);
                if (i < enemy_last_action_times.size()) {
                    enemy_last_action_times.erase(enemy_last_action_times.begin() + i);
                }
                break;
            }
        }

        actor.gold += 10;
        credit_kill(actor);
    }

    player_last_action_time = steps;
    steps++;
    check_victory_conditions();
    return result;
}

PickupResult State::collect_loot_at(Player& actor) {
    PickupResult result;
    if (state != GameState::Running) return result;

    auto it = dropped_loot.begin();
    while (it != dropped_loot.end()) {
        if (it->x == actor.x && it->y == actor.y) {
            if (it->item.id == "extra_life") {
                actor.respawns_remaining += std::max(1, it->item.active_value);
                result.items.push_back(it->item);
                it = dropped_loot.erase(it);
                result.success = true;
                continue;
            }

            auto inv_it = std::find_if(actor.inventory.begin(), actor.inventory.end(),
                [&it](const Item& inv_item) { return inv_item.id == it->item.id && inv_item.type == it->item.type; });

            if (inv_it != actor.inventory.end()) {
                inv_it->count += it->item.count;
            } else {
                actor.inventory.push_back(it->item);
            }

            actor.recalculate_passives();
            result.items.push_back(it->item);
            it = dropped_loot.erase(it);
            result.success = true;
        } else {
            ++it;
        }
    }

    return result;
}

UseItemResult State::use_item_actor(Player& actor, const std::string& item_id) {
    UseItemResult result;
    if (state != GameState::Running) return result;

    auto it = std::find_if(actor.inventory.begin(), actor.inventory.end(),
                           [&item_id](const Item& item) { return item.id == item_id; });

    if (it == actor.inventory.end()) return result;

    if (it->active_effect == "heal") {
        if (actor.hp < actor.max_hp) {
            result.success = true;
            result.effect = "heal";
            result.value = it->active_value;
            actor.hp = std::min(actor.max_hp, actor.hp + it->active_value);
            it->count--;
        }
    } else if (it->active_effect == "respawn") {
        result.success = true;
        result.effect = "respawn";
        result.value = std::max(1, it->active_value);
        actor.respawns_remaining += result.value;
        it->count--;
    }

    if (result.success) {
        if (it->count <= 0) {
            actor.inventory.erase(it);
            actor.recalculate_passives();
        }
        player_last_action_time = steps;
        steps++;
    }
    return result;
}

MoveResult State::move_slot(const std::string& slot, int dx, int dy) {
    Player* actor = actor_for_slot(slot);
    if (!actor) return {};

    auto result = move_actor(*actor, dx, dy, actor->x, actor->y);
    if (result.success) process_enemy_turns();
    return result;
}

MoveResult State::move_spectator(int dx, int dy) {
    MoveResult result;
    if (!god_mode || state != GameState::Running) return result;

    const int target_x = spectator.x + dx;
    const int target_y = spectator.y + dy;
    if (target_x < 0 || target_x >= map.width || target_y < 0 || target_y >= map.height) {
        return result;
    }

    spectator.x = target_x;
    spectator.y = target_y;
    result.success = true;
    result.x = spectator.x;
    result.y = spectator.y;
    return result;
}

AttackResult State::attack_slot(const std::string& slot, int dx, int dy) {
    Player* actor = actor_for_slot(slot);
    if (!actor) return {};

    auto result = attack_actor(*actor, dx, dy);
    if (result.success) process_enemy_turns();
    return result;
}

UseItemResult State::use_item_slot(const std::string& slot, const std::string& item_id) {
    Player* actor = actor_for_slot(slot);
    if (!actor) return {};

    auto result = use_item_actor(*actor, item_id);
    if (result.success) process_enemy_turns();
    return result;
}

void State::process_enemy_turns() {
    if (state != GameState::Running) return;

    std::vector<int> enemy_ids;
    for (const auto& e : enemies) enemy_ids.push_back(e.id);

    for (int id : enemy_ids) {
        if (state != GameState::Running) return;

        int idx = -1;
        for (size_t i = 0; i < enemies.size(); ++i) {
            if (enemies[i].id == id) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx < 0) continue;

        Enemy& enemy = enemies[idx];

        if (!is_enemy_active(enemy)) continue;

        if (enemy.type == EnemyType::Rat) {
            process_rat_turn(idx);
            continue;
        }

        int best_dist_sq = 999999;
        int target_x = player.x;
        int target_y = player.y;

        auto consider = [&](int x, int y, int hp) {
            if (hp <= 0) return;
            const int dx = x - enemy.x;
            const int dy = y - enemy.y;
            const int dist_sq = dx * dx + dy * dy;
            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                target_x = x;
                target_y = y;
            }
        };
        consider(player.x, player.y, player.hp);
        consider(rival.x, rival.y, rival.hp);

        const int dx = target_x - enemy.x;
        const int dy = target_y - enemy.y;
        const int dist_sq = dx * dx + dy * dy;

        if (dist_sq == 1 || dist_sq == 2) {
            int adx = 0;
            int ady = 0;
            if (std::abs(dx) >= std::abs(dy) && dx != 0) {
                adx = dx > 0 ? 1 : -1;
            } else if (dy != 0) {
                ady = dy > 0 ? 1 : -1;
            }
            enemy_attack(idx, adx, ady);
            continue;
        }

        const auto step = bfs_next_step(enemy.x, enemy.y, target_x, target_y);
        if (step.first != 0 || step.second != 0) {
            enemy_move(idx, step.first, step.second);
        }
    }

    check_victory_conditions();
}

// Враг движется
MoveResult State::enemy_move(int enemy_index, int dx, int dy) {
    MoveResult result;
    if (state != GameState::Running || enemy_index < 0 || enemy_index >= (int)enemies.size()) return result;

    Enemy& enemy = enemies[enemy_index];
    int target_x = enemy.x + dx;
    int target_y = enemy.y + dy;

    // Проверяем, можно ли там оказаться
    if (!map.is_walkable(target_x, target_y)) return result;
    
    // Проверяем, нет ли там другого врага
    if (has_enemy_at(target_x, target_y)) return result;
    
    // Block rival; human blocks only when not in god mode
    if (rival.hp > 0 && target_x == rival.x && target_y == rival.y) return result;
    if (player_blocks_world() && target_x == player.x && target_y == player.y) return result;

    move_enemy(enemy_index, target_x, target_y);
    result.success = true;
    result.x = target_x;
    result.y = target_y;

    enemy_last_action_times[enemy_index] = steps;
    steps++;
    return result;
}

// Враг атакует
AttackResult State::enemy_attack(int enemy_index, int dx, int dy) {
    AttackResult result;
    if (state != GameState::Running || enemy_index < 0 || enemy_index >= (int)enemies.size()) return result;

    Enemy& attacker = enemies[enemy_index];
    int target_x = attacker.x + dx;
    int target_y = attacker.y + dy;

    auto hit_player = [&](Player& target, int& ticks_since_death) {
        const int base_damage = enemy_stats_for(attacker.type).damage;
        const int damage = target.apply_resistance(base_damage);
        result.damage = damage;
        result.success = true;
        target.hp -= damage;
        if (target.hp <= 0) {
            target.hp = 0;
            ticks_since_death = 0;
        }
        enemy_last_action_times[enemy_index] = steps;
        steps++;
    };

    if (target_x == rival.x && target_y == rival.y && rival.hp > 0) {
        hit_player(rival, rival_ticks_since_death);
        return result;
    }

    if (target_x == player.x && target_y == player.y && player.hp > 0) {
        hit_player(player, player_ticks_since_death);
        return result;
    }

    // Проверяем, атакует ли другого врага
    Enemy* target = get_enemy_at(target_x, target_y);
    if (target) {
        // Враги не атакуют друг друга в этой системе
        return result;
    }

    return result;
}

// Проверка готовности действия игрока
bool State::is_player_action_resolved(int current_time_ms) const {
    return (current_time_ms - player_last_action_time) >= action_duration_ms;
}

// Проверка готовности действия врага
bool State::is_enemy_action_resolved(int enemy_index, int current_time_ms) const {
    if (enemy_index < 0 || enemy_index >= (int)enemy_last_action_times.size()) {
        return false;
    }
    return (current_time_ms - enemy_last_action_times[enemy_index]) >= action_duration_ms;
}

// Real-time обновление (вызывается из API)
// Каждое действие визуально занимает action_duration_ms на анимацию
// Клиент может отправлять следующее действие только когда action_resolved == true
void State::update(int current_time_ms) {
    if (state != GameState::Running) return;
    
    // Проверяем условия победы каждый update
    check_victory_conditions();
}

// Проверка условий победы
void State::check_victory_conditions() {
    if (state != GameState::Running) return;

    if (player_kills >= kills_required) {
        level_winner = LevelWinner::Human;
        state = GameState::LevelComplete;
        return;
    }
    if (rival_kills >= kills_required) {
        level_winner = LevelWinner::Ai;
        state = GameState::LevelComplete;
    }
}

json State::get_full_map_cells() const {
    json cells = json::array();
    for (int y = 0; y < map.height; ++y) {
        for (int x = 0; x < map.width; ++x) {
            const TileType tile = map.grid[y * map.width + x];
            cells.push_back({
                {"x", x},
                {"y", y},
                {"type", tile_type_name(tile)}
            });
        }
    }
    return cells;
}

json State::get_visible_cells(int radius, const std::string& slot) const {
    if (god_mode && slot == "spectator") {
        return get_full_map_cells();
    }
    const Player* actor = actor_for_slot(slot);
    if (!actor) return json::array();
    return get_visible_cells_for(actor->x, actor->y, radius);
}

json State::get_visible_cells_for(int cx, int cy, int radius) const {
    json cells = json::array();
    if (map.width <= 0 || map.height <= 0) return cells;

    const int clamped_radius = std::max(1, std::min(radius, 10));
    const int width = map.width;
    const int height = map.height;
    const auto index = [width](int x, int y) { return y * width + x; };

    std::vector<int> dist(width * height, -1);
    std::queue<std::pair<int, int>> q;

    if (cx < 0 || cx >= width || cy < 0 || cy >= height) return cells;
    if (!map.is_walkable(cx, cy)) return cells;

    dist[index(cx, cy)] = 0;
    q.push({cx, cy});

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    while (!q.empty()) {
        const auto [x, y] = q.front();
        q.pop();
        const int d = dist[index(x, y)];

        const TileType tile = map.grid[index(x, y)];
        cells.push_back({
            {"x", x},
            {"y", y},
            {"type", tile_type_name(tile)}
        });

        if (d >= clamped_radius) continue;

        for (const auto& dir : dirs) {
            const int nx = x + dir[0];
            const int ny = y + dir[1];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            if (!map.is_walkable(nx, ny)) continue;

            const int ni = index(nx, ny);
            if (dist[ni] != -1) continue;
            dist[ni] = d + 1;
            q.push({nx, ny});
        }
    }

    const int room_index = room_index_at(cx, cy);
    if (room_index >= 0) {
        const auto& room = map.rooms[static_cast<size_t>(room_index)];
        std::unordered_set<std::string> seen;
        for (const auto& cell : cells) {
            seen.insert(std::to_string(cell["x"].get<int>()) + "," +
                         std::to_string(cell["y"].get<int>()));
        }
        for (int y = room.y; y < room.y + room.h; ++y) {
            for (int x = room.x; x < room.x + room.w; ++x) {
                if (!map.is_walkable(x, y)) continue;
                const auto key = std::to_string(x) + "," + std::to_string(y);
                if (seen.count(key)) continue;
                seen.insert(key);
                cells.push_back({
                    {"x", x},
                    {"y", y},
                    {"type", tile_type_name(map.grid[index(x, y)])}
                });
            }
        }
    }

    return cells;
}

int State::room_index_at(int x, int y) const {
    for (size_t i = 0; i < map.rooms.size(); ++i) {
        const auto& room = map.rooms[i];
        if (x >= room.x && x < room.x + room.w && y >= room.y && y < room.y + room.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool State::corridor_links_room(size_t corridor_index, int room_index) const {
    if (room_index < 0 || corridor_index >= map.corridors.size()) return false;
    const auto& corridor = map.corridors[corridor_index];
    const auto& room = map.rooms[static_cast<size_t>(room_index)];

    for (int y = corridor.y; y < corridor.y + corridor.h; ++y) {
        for (int x = corridor.x; x < corridor.x + corridor.w; ++x) {
            if (!map.is_walkable(x, y)) continue;
            static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            for (const auto& dir : dirs) {
                const int nx = x + dir[0];
                const int ny = y + dir[1];
                if (nx >= room.x && nx < room.x + room.w &&
                    ny >= room.y && ny < room.y + room.h &&
                    map.is_walkable(nx, ny)) {
                    return true;
                }
            }
        }
    }
    return false;
}

int State::corridor_destination_room(size_t corridor_index, int from_room_index) const {
    if (from_room_index < 0 || corridor_index >= map.corridors.size()) return -1;
    for (size_t ri = 0; ri < map.rooms.size(); ++ri) {
        if (static_cast<int>(ri) == from_room_index) continue;
        if (corridor_links_room(corridor_index, static_cast<int>(ri))) {
            return static_cast<int>(ri);
        }
    }
    return -1;
}

int State::corridor_index_at(int x, int y) const {
    for (size_t i = 0; i < map.corridors.size(); ++i) {
        const auto& corridor = map.corridors[i];
        if (x >= corridor.x && x < corridor.x + corridor.w &&
            y >= corridor.y && y < corridor.y + corridor.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int State::entry_corridor_for_room(int room_index, int ax, int ay) const {
    if (room_index < 0 || room_index >= static_cast<int>(map.rooms.size())) return -1;

    const auto& room = map.rooms[static_cast<size_t>(room_index)];
    int best_ci = -1;
    int best_dist = 999999;

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    for (size_t ci = 0; ci < map.corridors.size(); ++ci) {
        if (!corridor_links_room(ci, room_index)) continue;

        const auto& corridor = map.corridors[ci];
        for (int y = corridor.y; y < corridor.y + corridor.h; ++y) {
            for (int x = corridor.x; x < corridor.x + corridor.w; ++x) {
                if (!map.is_walkable(x, y)) continue;

                bool touches_room = false;
                for (const auto& dir : dirs) {
                    const int nx = x + dir[0];
                    const int ny = y + dir[1];
                    if (nx >= room.x && nx < room.x + room.w &&
                        ny >= room.y && ny < room.y + room.h &&
                        map.is_walkable(nx, ny)) {
                        touches_room = true;
                        break;
                    }
                }
                if (!touches_room) continue;

                const int dist = std::abs(ax - x) + std::abs(ay - y);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_ci = static_cast<int>(ci);
                }
            }
        }
    }

    return best_ci;
}

std::vector<std::pair<int, int>> State::bfs_path_cells(
    int from_x, int from_y, int to_x, int to_y, bool block_actors) const {
    std::vector<std::pair<int, int>> empty;
    if (from_x == to_x && from_y == to_y) return empty;

    const int width = map.width;
    const int height = map.height;
    const auto index = [width](int x, int y) { return y * width + x; };
    const int start = index(from_x, from_y);
    const int goal = index(to_x, to_y);

    std::vector<int> parent(width * height, -1);
    std::queue<std::pair<int, int>> q;
    q.push({from_x, from_y});
    parent[start] = start;

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    auto walkable = [&](int x, int y) {
        if (!map.is_walkable(x, y)) return false;
        if (block_actors && has_enemy_at(x, y)) return false;
        if (block_actors && player_blocks_world() && player.x == x && player.y == y) return false;
        if (block_actors && rival.hp > 0 && rival.x == x && rival.y == y &&
            !(x == from_x && y == from_y)) return false;
        return true;
    };

    while (!q.empty()) {
        const auto [cx, cy] = q.front();
        q.pop();
        const int ci = index(cx, cy);

        if (ci == goal) {
            std::vector<std::pair<int, int>> path;
            int cur = goal;
            while (cur != start) {
                path.push_back({cur % width, cur / width});
                cur = parent[cur];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (const auto& dir : dirs) {
            const int nx = cx + dir[0];
            const int ny = cy + dir[1];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

            const int ni = index(nx, ny);
            if (parent[ni] != -1) continue;
            if (!walkable(nx, ny) && ni != goal) continue;

            parent[ni] = ci;
            q.push({nx, ny});
        }
    }

    return empty;
}

std::vector<std::string> State::cells_to_directions(
    int from_x, int from_y, const std::vector<std::pair<int, int>>& cells) const {
    std::vector<std::string> directions;
    int px = from_x;
    int py = from_y;
    for (const auto& [nx, ny] : cells) {
        const int dx = nx - px;
        const int dy = ny - py;
        if (dx == 1) directions.push_back("right");
        else if (dx == -1) directions.push_back("left");
        else if (dy == 1) directions.push_back("down");
        else if (dy == -1) directions.push_back("up");
        px = nx;
        py = ny;
    }
    return directions;
}

json State::scout_around(const std::string& slot,
                         int entry_corridor_hint,
                         const std::vector<int>& blocked_corridors) const {
    static constexpr int kScoutCorridorVision = 8;

    json out = {
        {"success", false},
        {"paths", json::array()},
        {"cells", json::array()},
        {"room_index", -1},
        {"entry_corridor_index", -1},
        {"linked_corridor_count", 0},
        {"session_id", session_id}
    };

    const Player* actor = actor_for_slot(slot);
    if (!actor || state != GameState::Running || actor->hp <= 0) return out;

    const int room_at = room_index_at(actor->x, actor->y);
    out["full_room_revealed"] = room_at >= 0;
    out["cells"] = get_visible_cells(kScoutCorridorVision, slot);

    int room_index = room_at;
    if (room_index < 0 && !map.rooms.empty()) {
        int best_dist = 999999;
        for (size_t i = 0; i < map.rooms.size(); ++i) {
            const auto& room = map.rooms[i];
            const int cx = room.x + room.w / 2;
            const int cy = room.y + room.h / 2;
            const int dist = std::abs(actor->x - cx) + std::abs(actor->y - cy);
            if (dist < best_dist) {
                best_dist = dist;
                room_index = static_cast<int>(i);
            }
        }
    }

    out["room_index"] = room_index;
    if (room_index < 0) {
        out["error"] = "no_room_found";
        return out;
    }

    std::unordered_set<int> blocked;
    for (int ci : blocked_corridors) {
        if (ci >= 0) blocked.insert(ci);
    }

    std::vector<int> linked_corridors;
    json candidate_paths = json::array();

    for (size_t ci = 0; ci < map.corridors.size(); ++ci) {
        if (!corridor_links_room(ci, room_index)) continue;
        linked_corridors.push_back(static_cast<int>(ci));

        if (blocked.count(static_cast<int>(ci)) > 0) continue;

        const auto& corridor = map.corridors[ci];
        int best_goal_x = -1;
        int best_goal_y = -1;
        size_t best_len = 999999;
        std::vector<std::pair<int, int>> best_cells;

        for (int y = corridor.y; y < corridor.y + corridor.h; ++y) {
            for (int x = corridor.x; x < corridor.x + corridor.w; ++x) {
                if (!map.is_walkable(x, y)) continue;
                if (room_index_at(x, y) == room_index) continue;

                const auto cells = bfs_path_cells(actor->x, actor->y, x, y, true);
                if (cells.empty()) continue;
                if (cells.size() >= best_len) continue;

                best_len = cells.size();
                best_goal_x = x;
                best_goal_y = y;
                best_cells = cells;
            }
        }

        if (best_goal_x < 0) continue;

        const auto directions = cells_to_directions(actor->x, actor->y, best_cells);
        if (directions.empty()) continue;

        candidate_paths.push_back({
            {"corridor_index", static_cast<int>(ci)},
            {"destination_room_index", corridor_destination_room(ci, room_index)},
            {"goal_x", best_goal_x},
            {"goal_y", best_goal_y},
            {"length", directions.size()},
            {"directions", directions},
            {"avoid_return", false},
            {"is_forced_exit", false}
        });
    }

    out["linked_corridor_count"] = linked_corridors.size();

    int entry_corridor = entry_corridor_hint;
    if (entry_corridor < 0) {
        entry_corridor = entry_corridor_for_room(room_index, actor->x, actor->y);
    }
    out["entry_corridor_index"] = entry_corridor;

    json paths = json::array();
    json forced_entry_path = nullptr;

    for (const auto& path : candidate_paths) {
        const int ci = path.value("corridor_index", -1);
        if (linked_corridors.size() > 1 && ci == entry_corridor) {
            continue;
        }
        if (linked_corridors.size() == 1 && ci == entry_corridor) {
            json marked = path;
            marked["avoid_return"] = true;
            marked["is_forced_exit"] = true;
            forced_entry_path = std::move(marked);
            continue;
        }
        paths.push_back(path);
    }

    if (paths.empty() && !forced_entry_path.is_null()) {
        paths.push_back(forced_entry_path);
        out["only_exit_marked_bad"] = true;
    } else {
        out["only_exit_marked_bad"] = false;
    }

    out["paths"] = paths;
    const bool has_paths = !paths.empty();
    const bool has_cells = !out["cells"].empty();
    out["success"] = has_paths || has_cells;
    if (!has_paths && has_cells) {
        out["warning"] = "no_corridor_paths";
    }
    if (!has_paths && !has_cells) {
        out["error"] = "nothing_to_scout";
    }
    return out;
}

// Legacy comment removed

std::vector<std::string> State::get_available_actions(const std::string& slot) const {
    std::vector<std::string> actions;

    auto append_query_tools = [&]() {
        actions.push_back("get_game_state");
        actions.push_back("get_available_actions");
        actions.push_back("scout_around");
    };

    const Player* actor = actor_for_slot(slot);
    if (!actor) return actions;

    if (state != GameState::Running) return actions;

    if (actor->hp <= 0) {
        append_query_tools();
        return actions;
    }

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    bool can_move = false;
    bool can_attack = false;

    for (const auto& dir : dirs) {
        const int tx = actor->x + dir[0];
        const int ty = actor->y + dir[1];
        if (has_enemy_at(tx, ty)) {
            can_attack = true;
        } else if (!is_cell_blocked(tx, ty, actor->x, actor->y)) {
            can_move = true;
        }
    }

    if (can_move) actions.push_back("move");
    if (can_attack) actions.push_back("attack");

    for (const auto& item : actor->inventory) {
        if (item.id == "potion" && actor->hp < actor->max_hp) {
            actions.push_back("use_item");
            break;
        }
    }

    append_query_tools();
    return actions;
}
json State::get_enemy_visible_cells(int enemy_index, int radius) const {
    json cells = json::array();
    if (enemy_index < 0 || enemy_index >= (int)enemies.size()) return cells;

    const Enemy& enemy = enemies[enemy_index];
    const int clamped_radius = std::max(1, std::min(radius, 10));

    for (int dy = -clamped_radius; dy <= clamped_radius; ++dy) {
        for (int dx = -clamped_radius; dx <= clamped_radius; ++dx) {
            if (dx * dx + dy * dy > clamped_radius * clamped_radius) continue;

            const int x = enemy.x + dx;
            const int y = enemy.y + dy;
            if (x < 0 || x >= map.width || y < 0 || y >= map.height) continue;

            const TileType tile = map.grid[y * map.width + x];
            cells.push_back({
                {"x", x},
                {"y", y},
                {"type", tile_type_name(tile)}
            });
        }
    }

    return cells;
}

// Получить состояние всех врагов
std::vector<json> State::get_enemies_state() const {
    std::vector<json> result;
    for (size_t i = 0; i < enemies.size(); ++i) {
        const Enemy& enemy = enemies[i];
        result.push_back(json{
            {"index", (int)i},
            {"id", enemy.id},
            {"x", enemy.x},
            {"y", enemy.y},
            {"hp", enemy.hp},
            {"type", enemy.type}
        });
    }
    return result;
}

void to_json(json& j, const State& state) {
    const bool game_over = is_game_over(state.state);
    const bool won = is_won(state.state);
    const bool level_complete = is_level_complete(state.state);
    j = json{
        {"enemies", state.get_enemies_state()},
        {"player", state.player},
        {"rival", state.rival},
        {"session_active", state.session_active},
        {"session_id", state.session_id},
        {"game_state", state.state},
        {"phase", game_state_to_phase(state.state)},
        {"game_over", game_over},
        {"won", won},
        {"level_complete", level_complete},
        {"campaign_level", state.campaign_level},
        {"player_kills", state.player_kills},
        {"rival_kills", state.rival_kills},
        {"kills_required", state.kills_required},
        {"max_enemies_on_level", state.max_enemies_on_level},
        {"level_winner", state.level_winner},
        {"god_mode", state.god_mode},
        {"spectator", state.spectator},
        {"steps", state.steps},
        {"map_loot", state.dropped_loot},
        {"rival_room_index", state.room_index_at(state.rival.x, state.rival.y)},
        {"player_room_index", state.room_index_at(state.player.x, state.player.y)}
    };
}

void from_json(const json& j, State& state) {
    if (j.contains("enemies")) j.at("enemies").get_to(state.enemies);
    if (j.contains("player")) j.at("player").get_to(state.player);
    if (j.contains("game_state")) j.at("game_state").get_to(state.state);
}

}
