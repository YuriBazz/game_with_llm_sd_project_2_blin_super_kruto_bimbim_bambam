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
        // Вместо перемещения запускается логика АТАКИ
        // trigger_attack_enemy_at(target_x, target_y);
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

State::State(utils::RandomGenerator& rng) : player{0, 0, 0, 0, 0}, phase(Phase::PlayerTurn), rng{rng} {}

void State::start_new_game(MapOptions options) {
    MapGenerator generator(rng);
    map = generator.generate_map(options);

    phase = Phase::PlayerTurn;

    // Спавним игрока в центре самой первой комнаты (комнаты гарантированно есть)
    if (!map.rooms.empty()) {
        player.x = map.rooms[0].x + map.rooms[0].w / 2;
        player.y = map.rooms[0].y + map.rooms[0].h / 2;
        player.max_hp = kPlayerMaxHp;
        player.hp = kPlayerMaxHp;
        player.gold = 0;
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
                Enemy enemy{enemy_id_counter, enemy_x, enemy_y, 10, EnemyType::Goblin};

                enemies.push_back(enemy);
                entity_grid[idx] = enemy_id_counter; // Заняли клетку на карте врагов

                enemy_id_counter++;
            }
        }
    }
}

void State::move_enemy(size_t enemy_index, int new_x, int new_y) {
    Enemy& enemy = enemies[enemy_index];

    // Освобождаем старую клетку в сетке
    entity_grid[enemy.y * map.width + enemy.x] = -1;

    // Обновляем координаты
    enemy.x = new_x;
    enemy.y = new_y;

    // Занимаем новую клетку
    entity_grid[new_y * map.width + new_x] = enemy.id;
}

void State::process_enemy_turn() {
    if (phase != Phase::EnemyTurn) return;

    for (size_t i = 0; i < enemies.size(); ++i) {
        auto& enemy = enemies[i];

        // Считаем расстояние до игрока (Манхэттенское расстояние)
        int dist_x = player.x - enemy.x;
        int dist_y = player.y - enemy.y;
        int distance = std::abs(dist_x) + std::abs(dist_y);

        if (distance == 1) {
            // Враг вплотную к игроку — Атакует!
            player.hp -= 2;
            if (player.hp <= 0) {
                phase = Phase::PlayerDead;
                return;
            }
        }
        else if (distance > 1 && distance <= 6) {
            // Простейший ИИ: Враг видит игрока в радиусе 6 клеток и делает шаг к нему
            int next_x = enemy.x + (dist_x > 0 ? 1 : (dist_x < 0 ? -1 : 0));
            int next_y = enemy.y + (dist_y > 0 ? 1 : (dist_y < 0 ? -1 : 0));

            // Проверяем, свободен ли путь для шага (геометрия + отсутствие других монстров)
            if (map.is_walkable(next_x, next_y) && !has_enemy_at(next_x, next_y)) {
                // Игрок не должен стоять на этой клетке
                if (next_x != player.x || next_y != player.y) {
                    move_enemy(i, next_x, next_y);
                }
            }
        }
    }

    // Если игрок выжил, возвращаем ему управление
    if (phase == Phase::EnemyTurn) {
        phase = Phase::PlayerTurn;
    }
}

void State::end_player_turn() {
    steps++;
    phase = Phase::EnemyTurn;
    process_enemy_turn();
}

MoveResult State::move(int dx, int dy) {
    MoveResult result;
    if (phase != Phase::PlayerTurn) return result;

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

    end_player_turn();
    return result;
}

AttackResult State::attack(int dx, int dy) {
    AttackResult result;
    if (phase != Phase::PlayerTurn) return result;

    int target_x = player.x + dx;
    int target_y = player.y + dy;

    Enemy* target = get_enemy_at(target_x, target_y);
    if (!target) return result;

    result.damage = kPlayerDamage;
    result.success = true;
    target->hp -= result.damage;

    if (target->hp <= 0) {
        result.target_dead = true;

        int chance = rng.get_random(0, 100);

        if (chance < 40) {
            drop_loot(target->x, target->y, {"potion", "Health Potion", "consumable", 1});
        } else if (chance > 85) {
            drop_loot(target->x, target->y, {"sword", "Iron Sword", "weapon", 1});
        }

        entity_grid[target->y * map.width + target->x] = -1;
        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(),
                           [target](const Enemy& e) { return e.id == target->id; }),
            enemies.end()
        );
        player.gold += 10;
    }

    end_player_turn();
    return result;
}

PickupResult State::pickup() {
    PickupResult result;
    if (phase != Phase::PlayerTurn) return result;

    auto it = dropped_loot.begin();
    while (it != dropped_loot.end()) {
        if (it->x == player.x && it->y == player.y) {
            auto inv_it = std::find_if(player.inventory.begin(), player.inventory.end(),
                [&it](const Item& inv_item) { return inv_item.id == it->item.id && inv_item.type == "consumable"; });

            if (inv_it != player.inventory.end()) {
                inv_it->count += it->item.count;
            } else {
                player.inventory.push_back(it->item);
            }

            result.items.push_back(it->item);
            it = dropped_loot.erase(it);
            result.success = true;
        } else {
            ++it;
        }
    }

    if (result.success) {
        end_player_turn();
    }
    return result;
}

UseItemResult State::use_item(const std::string& item_id) {
    UseItemResult result;
    if (phase != Phase::PlayerTurn) return result;

    auto it = std::find_if(player.inventory.begin(), player.inventory.end(),
                           [&item_id](const Item& item) { return item.id == item_id; });

    if (it == player.inventory.end()) return result;

    if (it->id == "potion") {
        if (player.hp < player.max_hp) {
            result.success = true;
            result.effect = "heal";
            result.value = kPotionHeal;
            player.hp = std::min(player.max_hp, player.hp + kPotionHeal);
            it->count--;
        }
    }

    if (result.success) {
        if (it->count <= 0) {
            player.inventory.erase(it);
        }
        end_player_turn();
    }
    return result;
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

std::vector<std::string> State::get_available_actions() const {
    std::vector<std::string> actions;
    if (phase != Phase::PlayerTurn) return actions;

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

void to_json(json& j, const State& state) {
    j = json{
        {"enemies", state.enemies},
        {"player", state.player},
        {"game_over", state.phase == Phase::PlayerDead},
        {"phase", state.phase},
        {"won", state.phase == Phase::Victory},
        {"steps", state.steps},
        {"map_loot", state.dropped_loot}
    };
}

void from_json(const json& j, State& state) {
    if (j.contains("enemies")) j.at("enemies").get_to(state.enemies);
    if (j.contains("player")) j.at("player").get_to(state.player);
    if (j.contains("phase")) j.at("phase").get_to(state.phase);
}

}
