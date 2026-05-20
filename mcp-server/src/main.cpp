#include "mcp_core.hpp"
#include <iostream>
#include <string>

using json = nlohmann::json;

int main() {
    mcp::McpServer server("roguelike-mcp", "1.0.0");

    server.register_tool("get_game_state",
        "Get current game state (player HP, position, inventory, game over status)",
        {{"type", "object"}, {"properties", json::object()}},
        [](const json&) -> json {
            return {{"player", {{"hp", 100}, {"pos", {5, 10}}}, {"game_over", false}}};
        });

    server.register_tool("move",
        "Move player in specified direction",
        {{"type", "object"}, {"properties", {
            {"direction", {{"type", "string"}, {"enum", {"up", "down", "left", "right"}}}}
        }}, {"required", {"direction"}}},
        [](const json& args) -> json {
            std::string dir = args["direction"];
            return {{"success", true}, {"new_pos", {5, 11}}, {"direction", dir}};
        });

    server.register_tool("attack",
        "Attack monster at coordinates",
        {{"type", "object"}, {"properties", {
            {"target_x", {{"type", "integer"}, {"minimum", 0}, {"maximum", 19}}},
            {"target_y", {{"type", "integer"}, {"minimum", 0}, {"maximum", 19}}}
        }}, {"required", {"target_x", "target_y"}}},
        [](const json& args) -> json {
            return {{"success", true}, {"damage", 15}, {"target_dead", false}};
        });

    server.register_tool("get_visible_cells",
        "Get visible cells within radius from player",
        {{"type", "object"}, {"properties", {
            {"radius", {{"type", "integer"}, {"default", 5}, {"minimum", 1}, {"maximum", 10}}}
        }}},
        [](const json& args) -> json {
            int radius = args.value("radius", 5);
            json cells = json::array();
            for (int i = -radius; i <= radius; i++) {
                for (int j = -radius; j <= radius; j++) {
                    if (i*i + j*j <= radius*radius) {
                        cells.push_back({{"x", 5 + i}, {"y", 10 + j}, {"type", "floor"}});
                    }
                }
            }
            return {{"cells", cells}};
        });

    server.register_tool("pickup_item",
        "Pick up item from current cell",
        {{"type", "object"}, {"properties", json::object()}},
        [](const json&) -> json {
            return {{"success", true}, {"item", {{"id", "potion"}, {"name", "Health Potion"}}}};
        });

    server.register_tool("use_item",
        "Use item from inventory",
        {{"type", "object"}, {"properties", {
            {"item_id", {{"type", "string"}}},
            {"target", {{"type", "string"}, {"optional", true}}}
        }}, {"required", {"item_id"}}},
        [](const json& args) -> json {
            std::string item_id = args["item_id"];
            return {{"success", true}, {"effect", "heal"}, {"value", 20}};
        });

    server.register_tool("get_available_actions",
        "Get list of possible actions in current state",
        {{"type", "object"}, {"properties", json::object()}},
        [](const json&) -> json {
            return {{"actions", {"move", "attack", "use_item", "pickup_item"}}};
        });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string response = server.process_message(line);
        std::cout << response << std::endl;
    }

    return 0;
}
