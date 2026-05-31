#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

#include "../include/game_service/map/MapGenerator.hpp"
#include "game_service/State.hpp"
#include "game_service/map/MapOptions.hpp"

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

    std::cout << "Game service running on http://0.0.0.0:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
