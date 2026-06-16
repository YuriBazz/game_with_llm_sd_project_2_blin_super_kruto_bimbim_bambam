#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <cstring>

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

bool env_god_mode_enabled() {
    const char* value = std::getenv("GODMODE");
    if (!value) return false;
    return std::strcmp(value, "1") == 0 ||
           std::strcmp(value, "true") == 0 ||
           std::strcmp(value, "TRUE") == 0 ||
           std::strcmp(value, "yes") == 0 ||
           std::strcmp(value, "YES") == 0;
}

std::string action_slot_from(const httplib::Request& req, const json& body) {
    if (body.contains("slot") && body["slot"].is_string()) {
        return body["slot"].get<std::string>();
    }
    if (req.has_param("slot")) {
        return req.get_param_value("slot");
    }
    return "player";
}

bool is_agent_request(const httplib::Request& req) {
    return req.get_header_value("X-Controlled-By") == "agent";
}

bool reject_human_actor_control(const game::State& state,
                                const httplib::Request& req,
                                const std::string& slot,
                                httplib::Response& res) {
    if (!state.god_mode || is_agent_request(req)) {
        return false;
    }
    if (slot != "player" && slot != "rival") {
        return false;
    }
    res.status = 403;
    res.set_content(
        R"({"success": false, "error": "Spectator mode: H/A are LLM-controlled. Use POST /api/spectator/move for observer."})",
        "application/json");
    return true;
}

} // namespace

int main() {
    httplib::Server svr;
    utils::RandomGenerator rng;

    game::State state(rng);
    state.god_mode = env_god_mode_enabled();
    if (state.god_mode) {
        std::cout << "GODMODE enabled: spectator entity separate from robots H and A" << std::endl;
    } else {
        std::cout << "GODMODE disabled: set GODMODE=true for spectator (full map + observer O)"
                  << std::endl;
    }
    std::string map_snapshot;

    svr.Get("/health", [&state](const httplib::Request&, httplib::Response& res) {
        json body = {{"status", "ok"}, {"service", "game-service"}, {"god_mode", state.god_mode}};
        res.set_content(body.dump(), "application/json");
    });

    svr.Get("/api/state", [&state](const httplib::Request&, httplib::Response& res) {
        res.set_content(json(state).dump(), "application/json");
    });

    svr.Post("/api/tick", [&state](const httplib::Request& req, httplib::Response& res) {
        state.process_realtime_tick();
        json payload = {{"success", true}};
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    svr.Post("/api/map", [&state, &map_snapshot](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const std::string mode = body.value("mode", "start");
            if (mode == "reset") {
                state.reset_campaign();
            } else if (mode == "next_level") {
                state.advance_campaign_level();
            }
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
        const std::string slot = req.has_param("slot") ? req.get_param_value("slot") : "player";

        json payload = {
            {"success", true},
            {"cells", state.get_visible_cells(radius, slot)}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    svr.Get("/api/available_actions", [&state](const httplib::Request& req, httplib::Response& res) {
        const std::string slot = req.has_param("slot") ? req.get_param_value("slot") : "player";
        json payload = {
            {"success", true},
            {"actions", state.get_available_actions(slot)}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

    svr.Post("/api/spectator/move", [&](const httplib::Request& req, httplib::Response& res) {
        if (!state.god_mode) {
            res.status = 403;
            res.set_content(R"({"success": false, "error": "Spectator mode disabled"})", "application/json");
            return;
        }
        try {
            const json body = json::parse(req.body);
            const std::string direction = body.value("direction", "");
            int dx = 0;
            int dy = 0;
            get_dx_dy(direction, dx, dy);
            const game::MoveResult result = state.move_spectator(dx, dy);
            json payload = {
                {"success", result.success},
                {"x", result.x},
                {"y", result.y},
                {"entity", "spectator"}
            };
            res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"success": false, "error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/move", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const json body = json::parse(req.body);
            const std::string direction = body.value("direction", "unknown");
            const std::string slot = action_slot_from(req, body);
            if (reject_human_actor_control(state, req, slot, res)) {
                return;
            }

            int dx = 0;
            int dy = 0;
            get_dx_dy(direction, dx, dy);

            game::MoveResult result;
            if (dx != 0 || dy != 0) {
                result = state.move_slot(slot, dx, dy);
            }

            json payload = {
                {"success", result.success},
                {"x", result.x},
                {"y", result.y},
                {"slot", slot}
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
            const std::string slot = action_slot_from(req, body);
            if (slot != "player" && slot != "rival") {
                res.status = 400;
                res.set_content(R"({"success": false, "error": "Invalid slot"})", "application/json");
                return;
            }
            if (reject_human_actor_control(state, req, slot, res)) {
                return;
            }
            const int actor_x = (slot == "rival") ? state.rival.x : state.player.x;
            const int actor_y = (slot == "rival") ? state.rival.y : state.player.y;

            const int target_x = body.value("target_x", actor_x);
            const int target_y = body.value("target_y", actor_y);

            const int dx = target_x - actor_x;
            const int dy = target_y - actor_y;
            const game::AttackResult result = state.attack_slot(slot, dx, dy);

            json payload = {
                {"success", result.success},
                {"damage", result.damage},
                {"target_dead", result.target_dead},
                {"slot", slot}
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
            const std::string slot = action_slot_from(req, body);
            if (slot == "player" || slot == "rival") {
                if (reject_human_actor_control(state, req, slot, res)) {
                    return;
                }
            }

            game::UseItemResult result;
            if (!item_id.empty()) {
                result = state.use_item_slot(slot, item_id);
            }

            json payload = {{"success", result.success}, {"slot", slot}};
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

    svr.Get("/api/scout_around", [&state](const httplib::Request& req, httplib::Response& res) {
        const std::string slot = req.has_param("slot") ? req.get_param_value("slot") : "rival";
        int entry_corridor = -1;
        if (req.has_param("entry_corridor")) {
            try {
                entry_corridor = std::stoi(req.get_param_value("entry_corridor"));
            } catch (...) {}
        }
        std::vector<int> blocked;
        if (req.has_param("blocked")) {
            const std::string raw = req.get_param_value("blocked");
            size_t start = 0;
            while (start < raw.size()) {
                const size_t comma = raw.find(',', start);
                const std::string token = raw.substr(
                    start, comma == std::string::npos ? std::string::npos : comma - start);
                if (!token.empty()) {
                    try {
                        blocked.push_back(std::stoi(token));
                    } catch (...) {}
                }
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        json payload = state.scout_around(slot, entry_corridor, blocked);
        res.set_content(with_state(payload, state, false).dump(), "application/json");
    });

    svr.Get("/api/enemies", [&state](const httplib::Request& req, httplib::Response& res) {
        json payload = {
            {"success", true},
            {"enemies", state.get_enemies_state()}
        };
        res.set_content(with_state(payload, state, include_state(req)).dump(), "application/json");
    });

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
