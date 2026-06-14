#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

#include "game_service/map/MapGenerator.hpp"
#include "game_service/State.hpp"
#include "game_service/map/MapOptions.hpp"
#include "game_service/entities/Enemy.hpp"

using json = nlohmann::json;

namespace {

bool include_state(const httplib::Request& req) {
    if (req.has_param("include_state")) {
        return req.get_param_value("include_state") != "false";
    }
    return true;
}

json with_state(json payload, const game::State& state, bool attach_state) {
    if (attach_state) {
        payload["state"] = state;
    }
    return payload;
}

void get_dx_dy(const std::string& dir, int& dx, int& dy) {
    dx = 0;
    dy = 0;
    if (dir == "up") dy = -1;
    else if (dir == "down") dy = 1;
    else if (dir == "left") dx = -1;
    else if (dir == "right") dx = 1;
}

} // namespace

int main() {
    httplib::Server svr;
    utils::RandomGenerator rng;

    game::State state(rng);
    std::string map_snapshot;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status": "ok", "service": "game-service"})", "application/json");
    });

    svr.Get("/api/state", [&state](const httplib::Request&, httplib::Response& res) {
        res.set_content(json(state).dump(), "application/json");
    });

    svr.Post("/api/map", [&state, &map_snapshot](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const auto options = body.get<game::MapOptions>();
            state.start_new_game(options);

            const json map_data = state.map;
            map_snapshot = map_data.dump();
            res.set_content(with_state({{"success", true}}, state, include_state(req)).dump(),
                            "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Get("/api/map", [&map_snapshot](const httplib::Request&, httplib::Response& res) {
        res.set_content(map_snapshot, "application/json");
    });

    svr.Get("/api/visible_cells", [&state](const httplib::Request& req, httplib::Response& res) {
        int radius = 5;
        if (req.has_param("radius")) {
            radius = std::stoi(req.get_param_value("radius"));
        }

        json payload = {
            {"success", true},
            {"cells", state.get_visible_cells(radius)}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    svr.Get("/api/available_actions", [&state](const httplib::Request& req, httplib::Response& res) {
        json payload = {
            {"success", true},
            {"actions", state.get_available_actions()}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    svr.Post("/api/move", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const std::string direction = body.value("direction", "unknown");

            int dx = 0;
            int dy = 0;
            get_dx_dy(direction, dx, dy);

            game::MoveResult result;
            if (dx != 0 || dy != 0) {
                result = state.move(dx, dy);
            }

            json payload = {
                {"success", result.success},
                {"x", result.x},
                {"y", result.y}
            };
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/attack", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const int target_x = body.value("target_x", state.player.x);
            const int target_y = body.value("target_y", state.player.y);

            const int dx = target_x - state.player.x;
            const int dy = target_y - state.player.y;
            const game::AttackResult result = state.attack(dx, dy);

            json payload = {
                {"success", result.success},
                {"damage", result.damage},
                {"target_dead", result.target_dead}
            };
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/use_item", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const std::string item_id = body.value("item_id", "");

            game::UseItemResult result;
            if (!item_id.empty()) {
                result = state.use_item(item_id);
            }

            json payload = {{"success", result.success}};
            if (result.success) {
                payload["effect"] = result.effect;
                payload["value"] = result.value;
            }
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/pickup_item", [&](const httplib::Request& req, httplib::Response& res) {
        const game::PickupResult result = state.pickup();
        json payload = {
            {"success", result.success},
            {"items", result.items}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    // API для управления врагами
    
    // GET /api/enemies - получить список всех врагов
    svr.Get("/api/enemies", [&state](const httplib::Request& req, httplib::Response& res) {
        json payload = {
            {"success", true},
            {"enemies", state.get_enemies_state()}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    // POST /api/enemy/move - движение врага
    // Ожидает: {"enemy_index": <int>, "direction": "up|down|left|right"}
    svr.Post("/api/enemy/move", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            int enemy_index = body.value("enemy_index", -1);
            std::string direction = body.value("direction", "unknown");

            int dx = 0;
            int dy = 0;
            get_dx_dy(direction, dx, dy);

            game::MoveResult result;
            if (dx != 0 || dy != 0) {
                result = state.enemy_move(enemy_index, dx, dy);
            }

            json payload = {
                {"success", result.success},
                {"x", result.x},
                {"y", result.y}
            };
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    // POST /api/enemy/attack - атака врага
    // Ожидает: {"enemy_index": <int>, "target_x": <int>, "target_y": <int>}
    svr.Post("/api/enemy/attack", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            int enemy_index = body.value("enemy_index", -1);
            int target_x = body.value("target_x", 0);
            int target_y = body.value("target_y", 0);

            if (enemy_index < 0 || enemy_index >= (int)state.enemies.size()) {
                res.status = 400;
                res.set_content(R"({"success": false, "error": "Invalid enemy index"})", "application/json");
                return;
            }

            const Enemy& enemy = state.enemies[enemy_index];
            int dx = target_x - enemy.x;
            int dy = target_y - enemy.y;

            game::AttackResult result = state.enemy_attack(enemy_index, dx, dy);

            json payload = {
                {"success", result.success},
                {"damage", result.damage},
                {"target_dead", result.target_dead}
            };
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    // GET /api/enemy/visible_cells - видимость врага
    // Ожидает query param: ?enemy_index=<int>&radius=<int>
    svr.Get("/api/enemy/visible_cells", [&state](const httplib::Request& req, httplib::Response& res) {
        int enemy_index = -1;
        int radius = 5;

        if (req.has_param("enemy_index")) {
            enemy_index = std::stoi(req.get_param_value("enemy_index"));
        }
        if (req.has_param("radius")) {
            radius = std::stoi(req.get_param_value("radius"));
        }

        json payload = {
            {"success", enemy_index >= 0 && enemy_index < (int)state.enemies.size()},
            {"cells", state.get_enemy_visible_cells(enemy_index, radius)}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    // Real-Time Sync

    // GET /api/update - обновить состояние и проверить условия победы
    // Query param: ?current_time_ms=<int>
    svr.Get("/api/update", [&state](const httplib::Request& req, httplib::Response& res) {
        int current_time_ms = 0;
        if (req.has_param("current_time_ms")) {
            current_time_ms = std::stoi(req.get_param_value("current_time_ms"));
        }

        state.update(current_time_ms);

        json payload = {
            {"success", true},
            {"game_state", state.state}
        };
        res.set_content(with_state(payload, state, true).dump(), "application/json");
    });

    // GET /api/player/ready - проверить готовность игрока к действию
    // Query param: ?current_time_ms=<int>
    // Возвращает: {"ready": true/false, "time_to_action_ms": <int>}
    svr.Get("/api/player/ready", [&state](const httplib::Request& req, httplib::Response& res) {
        int current_time_ms = 0;
        if (req.has_param("current_time_ms")) {
            current_time_ms = std::stoi(req.get_param_value("current_time_ms"));
        }

        bool is_ready = state.is_player_action_resolved(current_time_ms);
        int time_to_action = state.action_duration_ms - (current_time_ms - state.player_last_action_time);
        if (time_to_action < 0) time_to_action = 0;

        json payload = {
            {"ready", is_ready},
            {"time_to_action_ms", time_to_action}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    // GET /api/enemy/ready - проверить готовность врага к действию
    // Query params: ?enemy_index=<int>&current_time_ms=<int>
    svr.Get("/api/enemy/ready", [&state](const httplib::Request& req, httplib::Response& res) {
        int enemy_index = -1;
        int current_time_ms = 0;

        if (req.has_param("enemy_index")) {
            enemy_index = std::stoi(req.get_param_value("enemy_index"));
        }
        if (req.has_param("current_time_ms")) {
            current_time_ms = std::stoi(req.get_param_value("current_time_ms"));
        }

        if (enemy_index < 0 || enemy_index >= (int)state.enemies.size()) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid enemy index"})", "application/json");
            return;
        }

        bool is_ready = state.is_enemy_action_resolved(enemy_index, current_time_ms);
        int time_to_action = state.action_duration_ms - (current_time_ms - state.enemy_last_action_times[enemy_index]);
        if (time_to_action < 0) time_to_action = 0;

        json payload = {
            {"ready", is_ready},
            {"time_to_action_ms", time_to_action}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    std::cout << "Game service running on http://0.0.0.0:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
