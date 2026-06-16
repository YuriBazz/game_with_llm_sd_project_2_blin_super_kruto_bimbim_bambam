#include <cassert>
#include <cmath>
#include <game_service/State.hpp>
#include <game_service/entities/Enemy.hpp>
#include <game_service/map/MapOptions.hpp>

int main() {
    utils::RandomGenerator rng;
    game::State state(rng);

    game::MapOptions options(30, 30, 8, 3, 123);
    state.start_new_game(options);

    assert(state.steps == 0);
    assert(state.player.hp == game::kPlayerMaxHp);
    assert(state.player.max_hp == game::kPlayerMaxHp);

    const int start_x = state.player.x;
    const int start_y = state.player.y;
    const auto move_result = state.move(0, 1);
    if (move_result.success) {
        assert(state.player.y == start_y + 1);
    }

    for (const auto& enemy : state.enemies) {
        if (enemy.type == EnemyType::Goblin) {
            assert(enemy.hp == 5);
        } else if (enemy.type == EnemyType::Orc) {
            assert(enemy.hp == 10);
        } else if (enemy.type == EnemyType::Troll) {
            assert(enemy.hp == 18);
        } else if (enemy.type == EnemyType::Rat) {
            assert(enemy.hp == 3);
        }
    }

    const int player_room = state.room_index_at(state.player.x, state.player.y);
    if (player_room >= 0) {
        bool enemy_in_player_room = false;
        for (const auto& enemy : state.enemies) {
            if (state.room_index_at(enemy.x, enemy.y) == player_room) {
                enemy_in_player_room = true;
                break;
            }
        }
        assert(enemy_in_player_room);
    }

    nlohmann::json serialized;
    game::to_json(serialized, state);
    assert(serialized.contains("game_over"));
    assert(serialized.contains("phase"));
    assert(serialized.contains("won"));
    assert(serialized.contains("player"));
    assert(serialized.contains("rival"));
    assert(serialized.contains("rival_kills"));
    assert(!serialized.contains("ai_player"));
    assert(serialized.contains("session_active"));
    assert(serialized.contains("campaign_level"));
    assert(serialized.contains("player_kills"));
    assert(serialized.contains("kills_required"));
    assert(serialized["game_state"] == "running");

    return 0;
}
