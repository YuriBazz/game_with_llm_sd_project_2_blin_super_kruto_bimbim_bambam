#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <random>
#include <ctime>

using json = nlohmann::json;

// Генерация случайной карты (заглушка для первого дедлайна)
json generate_map_stub() {
    return {
        {"width", 20},
        {"height", 20},
        {"rooms", {
            {{"x", 2}, {"y", 2}, {"w", 5}, {"h", 5}},
            {{"x", 10}, {"y", 8}, {"w", 6}, {"h", 4}}
        }}
    };
}

// Текущее состояние игры (заглушка)
json get_game_state() {
    static int step = 0;
    step++;

    return {
        {"player", {
            {"hp", 100},
            {"max_hp", 100},
            {"pos", {5 + step % 3, 10}},
            {"level", 1},
            {"xp", 0}
        }},
        {"inventory", {
            {{"id", "sword"}, {"name", "Iron Sword"}, {"type", "weapon"}},
            {{"id", "potion"}, {"name", "Health Potion"}, {"type", "consumable"}, {"count", 3}}
        }},
        {"game_over", false},
        {"won", false},
        {"steps", step}
    };
}

int main() {
    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status": "ok", "service": "game-service"})", "application/json");
    });

    svr.Get("/api/state", [](const httplib::Request&, httplib::Response& res) {
        json state = get_game_state();
        res.set_content(state.dump(), "application/json");
    });

    svr.Get("/api/map", [](const httplib::Request&, httplib::Response& res) {
        json map_data = generate_map_stub();
        res.set_content(map_data.dump(), "application/json");
    });

    svr.Post("/api/move", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
            return;
        }

        std::string direction = body.value("direction", "unknown");
        json response = {
            {"success", true},
            {"new_pos", {5, 11}},
            {"direction", direction},
            {"damage_taken", 0}
        };
        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/attack", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            int target_x = body.value("target_x", 0);
            int target_y = body.value("target_y", 0);

            json response = {
                {"success", true},
                {"damage", 15},
                {"target_hp_left", 5},
                {"target_dead", false}
            };
            res.set_content(response.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/use_item", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string item_id = body.value("item_id", "");

            json response = {
                {"success", true},
                {"effect", "heal"},
                {"value", 20}
            };
            res.set_content(response.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/pickup", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"success", true},
            {"item", {{"id", "potion"}, {"name", "Health Potion"}}}
        };
        res.set_content(response.dump(), "application/json");
    });

    std::cout << "Game service running on http://0.0.0.0:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
