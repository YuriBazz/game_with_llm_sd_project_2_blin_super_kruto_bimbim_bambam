//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/State.hpp>

#include "game_service/map/MapGenerator.hpp"
#include "game_service/map/TileType.hpp"

namespace game {

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

State::State(utils::RandomGenerator& rng) : map(), player{0, 0, 0, 0, 0}, state(GameState::Running), rng{rng} {
}

void State::start_new_game(const MapOptions &options) {
    // Очищаем состояние для новой игры
    enemies.clear();
    dropped_loot.clear();
    player.inventory.clear();
    player.current_damage_bonus = 0;
    player.current_resistance = 0;
    next_loot_id = 0;
    steps = 0;
    state = GameState::Running;
    player_last_action_time = 0;
    enemy_last_action_times.clear();

    MapGenerator generator(rng);
    map = generator.generate_map(options);

    // Спавним игрока в центре самой первой комнаты
    if (!map.rooms.empty()) {
        player.x = map.rooms[0].x + map.rooms[0].w / 2;
        player.y = map.rooms[0].y + map.rooms[0].h / 2;
        player.max_hp = kPlayerMaxHp;
        player.hp = kPlayerMaxHp;
        player.gold = 0;
        player.recalculate_passives();
    }

    // Инициализируем сетку врагов значениями -1 (пусто)
    entity_grid.assign(map.width * map.height, -1);

    int enemy_id_counter = 0;

    for (size_t i = 1; i < map.rooms.size(); ++i) {
        const auto& room = map.rooms[i];
        int enemies_count = rng.get_random(0, 3);

        for (int e = 0; e < enemies_count; ++e) {
            int enemy_x = room.x + rng.get_random(0, room.w - 1);
            int enemy_y = room.y + rng.get_random(0, room.h - 1);
            int idx = enemy_y * map.width + enemy_x;

            // Проверяем по сетке: если там уже кто-то есть или это стена — пропускаем
            if (map.grid[idx] == TileType::Floor && entity_grid[idx] == -1) {
                Enemy enemy{enemy_id_counter, enemy_x, enemy_y, 10, rng.get_random_enum<EnemyType>()};

                enemies.push_back(enemy);
                entity_grid[idx] = enemy_id_counter;
                enemy_last_action_times.push_back(0);

                enemy_id_counter++;
            }
        }
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

// Игрок движется
MoveResult State::move(int dx, int dy) {
    MoveResult result;
    if (state != GameState::Running) return result;

    int target_x = player.x + dx;
    int target_y = player.y + dy;

    if (has_enemy_at(target_x, target_y) || !map.is_walkable(target_x, target_y)) {
        return result;
    }

    player.x = target_x;
    player.y = target_y;
    result.success = true;
    result.x = player.x;
    result.y = player.y;

    player_last_action_time = steps;
    steps++;
    return result;
}

// Игрок атакует
AttackResult State::attack(int dx, int dy) {
    AttackResult result;
    if (state != GameState::Running) return result;

    int target_x = player.x + dx;
    int target_y = player.y + dy;

    Enemy* target = get_enemy_at(target_x, target_y);
    if (!target) return result;

    // Применить пассивные бонусы урона
    result.damage = player.get_total_damage(kPlayerDamage);
    result.success = true;
    target->hp -= result.damage;

    if (target->hp <= 0) {
        result.target_dead = true;

        int chance = rng.get_random(0, 100);

        if (chance < 40) {
            // Зелье восстановления (активный эффект)
            Item potion{
                "potion", "Health Potion", "consumable", 1,
                0, 0,  // no passive
                "heal", kPotionHeal  // active: heal +20
            };
            drop_loot(target->x, target->y, potion);
        } else if (chance > 85) {
            // Меч (пассивный бонус урона: +2)
            Item sword{
                "sword", "Iron Sword", "weapon", 1,
                2, 0,  // passive: +2 damage
                "", 0  // no active
            };
            drop_loot(target->x, target->y, sword);
        } else if (chance > 50) {
            // Броня (пассивный бонус сопротивления: +20%)
            Item armor{
                "armor", "Steel Armor", "armor", 1,
                0, 20,  // passive: +20% resistance
                "", 0   // no active
            };
            drop_loot(target->x, target->y, armor);
        }

        entity_grid[target->y * map.width + target->x] = -1;
        
        // Находим индекс врага для удаления
        for (size_t i = 0; i < enemies.size(); ++i) {
            if (enemies[i].id == target->id) {
                enemies.erase(enemies.begin() + i);
                if (i < enemy_last_action_times.size()) {
                    enemy_last_action_times.erase(enemy_last_action_times.begin() + i);
                }
                break;
            }
        }
        
        player.gold += 10;
    }

    player_last_action_time = steps;
    steps++;
    check_victory_conditions();
    return result;
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
    
    // Проверяем, нет ли там игрока
    if (target_x == player.x && target_y == player.y) return result;

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

    // Проверяем, атакует ли игрока
    if (target_x == player.x && target_y == player.y) {
        int base_damage = 2; // Урон врага
        // Применить сопротивление игрока к урону врага
        int damage = player.apply_resistance(base_damage);
        result.damage = damage;
        result.success = true;
        player.hp -= damage;

        if (player.hp <= 0) {
            state = GameState::EnemyVictory;
        }

        enemy_last_action_times[enemy_index] = steps;
        steps++;
        check_victory_conditions();
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

PickupResult State::pickup() {
    PickupResult result;
    if (state != GameState::Running) return result;

    auto it = dropped_loot.begin();
    while (it != dropped_loot.end()) {
        if (it->x == player.x && it->y == player.y) {
            // Лут доступен ТОЛЬКО герою, врагами не подбирается
            auto inv_it = std::find_if(player.inventory.begin(), player.inventory.end(),
                [&it](const Item& inv_item) { return inv_item.id == it->item.id && inv_item.type == it->item.type; });

            if (inv_it != player.inventory.end()) {
                inv_it->count += it->item.count;
            } else {
                player.inventory.push_back(it->item);
            }

            // Пересчитать пассивные бонусы после добавления предмета
            player.recalculate_passives();

            result.items.push_back(it->item);
            it = dropped_loot.erase(it);
            result.success = true;
        } else {
            ++it;
        }
    }

    if (result.success) {
        player_last_action_time = steps;
        steps++;
    }
    return result;
}

UseItemResult State::use_item(const std::string& item_id) {
    UseItemResult result;
    if (state != GameState::Running) return result;

    auto it = std::find_if(player.inventory.begin(), player.inventory.end(),
                           [&item_id](const Item& item) { return item.id == item_id; });

    if (it == player.inventory.end()) return result;

    // Активные эффекты работают только для зелий
    if (it->active_effect == "heal") {
        if (player.hp < player.max_hp) {
            result.success = true;
            result.effect = "heal";
            result.value = it->active_value;
            player.hp = std::min(player.max_hp, player.hp + it->active_value);
            it->count--;
        }
    }
    // Пассивные предметы нельзя использовать (они работают автоматически)
    // Если это оружие/броня, игрок должен просто иметь их в инвентаре

    if (result.success) {
        if (it->count <= 0) {
            player.inventory.erase(it);
            // Пересчитать пассивные бонусы после удаления предмета
            player.recalculate_passives();
        }
        player_last_action_time = steps;
        steps++;
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
    if (state == GameState::Running) {
        if (enemies.empty()) {
            state = GameState::PlayerVictory;
        } else if (player.hp <= 0) {
            state = GameState::EnemyVictory;
        }
    }
}

json State::get_visible_cells(int radius) const {
    json cells = json::array();
    const int clamped_radius = std::max(1, std::min(radius, 10));

    for (int dy = -clamped_radius; dy <= clamped_radius; ++dy) {
        for (int dx = -clamped_radius; dx <= clamped_radius; ++dx) {
            if (dx * dx + dy * dy > clamped_radius * clamped_radius) continue;

            const int x = player.x + dx;
            const int y = player.y + dy;
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

// Видимость для конкретного врага
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

std::vector<std::string> State::get_available_actions() const {
    std::vector<std::string> actions;
    if (state != GameState::Running) return actions;

    static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    bool can_move = false;
    bool can_attack = false;

    for (const auto& dir : dirs) {
        const int tx = player.x + dir[0];
        const int ty = player.y + dir[1];
        if (has_enemy_at(tx, ty)) {
            can_attack = true;
        } else if (map.is_walkable(tx, ty)) {
            can_move = true;
        }
    }

    if (can_move) actions.push_back("move");
    if (can_attack) actions.push_back("attack");

    for (const auto& loot : dropped_loot) {
        if (loot.x == player.x && loot.y == player.y) {
            actions.push_back("pickup_item");
            break;
        }
    }

    for (const auto& item : player.inventory) {
        if (item.id == "potion" && player.hp < player.max_hp) {
            actions.push_back("use_item");
            break;
        }
    }

    return actions;
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
    j = json{
        {"enemies", state.get_enemies_state()},
        {"player", state.player},
        {"game_state", state.state},
        {"steps", state.steps},
        {"map_loot", state.dropped_loot}
    };
}

void from_json(const json& j, State& state) {
    if (j.contains("enemies")) j.at("enemies").get_to(state.enemies);
    if (j.contains("player")) j.at("player").get_to(state.player);
    if (j.contains("game_state")) j.at("game_state").get_to(state.state);
}

}
