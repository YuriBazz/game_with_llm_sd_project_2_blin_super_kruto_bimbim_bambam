#include "mcp_core.hpp"
#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <memory>
#include <curl/curl.h>

using json = nlohmann::json;

// Callback for CURL to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper class to communicate with game-service using curl
class GameServiceClient {
private:
    std::string host_;
    int port_;
    std::string agent_slot_;

    static std::string agent_slot_from_env() {
        const char* slot = std::getenv("AGENT_SLOT");
        if (slot && (std::string(slot) == "player" || std::string(slot) == "rival")) {
            return slot;
        }
        return "rival";
    }
    
    std::string make_request(const std::string& method, const std::string& path, 
                            const std::string& body = "", bool is_json = false) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return "";
        }

        std::string url = "http://" + host_ + ":" + std::to_string(port_) + path;
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "X-Controlled-By: agent");
        if (is_json) {
            headers = curl_slist_append(headers, "Content-Type: application/json");
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }
        }
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return "";
        }
        return response;
    }
    
public:
    GameServiceClient(const std::string& host = "localhost", int port = 8080)
        : host_(host), port_(port), agent_slot_(agent_slot_from_env()) {
        const char* env_host = std::getenv("GAME_SERVICE_HOST");
        const char* env_port = std::getenv("GAME_SERVICE_PORT");

        if (env_host) host_ = env_host;
        if (env_port) port_ = std::stoi(env_port);
    }
    
    json get_state() {
        std::string response = make_request("GET", "/api/state");
        if (response.empty()) {
            return {{"error", "Failed to get game state"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"error", "Failed to parse game state"}};
        }
    }
    
    json move(const std::string& direction) {
        json body = {{"direction", direction}, {"slot", agent_slot_}};
        std::string response = make_request("POST", "/api/move", body.dump(), true);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to move"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse move response"}};
        }
    }
    
    json attack(int target_x, int target_y) {
        json body = {{"target_x", target_x}, {"target_y", target_y}, {"slot", agent_slot_}};
        std::string response = make_request("POST", "/api/attack", body.dump(), true);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to attack"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse attack response"}};
        }
    }
    
    json get_visible_cells(int radius = 5) {
        std::string path = "/api/visible_cells?radius=" + std::to_string(radius) +
                           "&slot=" + agent_slot_;
        std::string response = make_request("GET", path);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to get visible cells"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse visible cells response"}};
        }
    }
    
    json use_item(const std::string& item_id) {
        json body = {{"item_id", item_id}, {"slot", agent_slot_}};
        std::string response = make_request("POST", "/api/use_item", body.dump(), true);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to use item"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse use_item response"}};
        }
    }
    
    json scout_around(const json& args = json::object()) {
        std::string path = std::string("/api/scout_around?slot=") + agent_slot_;
        if (args.contains("entry_corridor") && args["entry_corridor"].is_number_integer()) {
            path += "&entry_corridor=" + std::to_string(args["entry_corridor"].get<int>());
        }
        if (args.contains("blocked_corridors") && args["blocked_corridors"].is_array()) {
            std::string blocked;
            for (const auto& item : args["blocked_corridors"]) {
                if (!item.is_number_integer()) continue;
                if (!blocked.empty()) blocked += ',';
                blocked += std::to_string(item.get<int>());
            }
            if (!blocked.empty()) {
                path += "&blocked=" + blocked;
            }
        }
        std::string response = make_request("GET", path);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to scout around"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse scout_around response"}};
        }
    }

    json get_available_actions() {
        std::string response = make_request("GET", "/api/available_actions?slot=" + agent_slot_);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to get available actions"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse available actions response"}};
        }
    }

    json new_game(const json& options = json::object()) {
        json body = options;
        if (body.empty()) {
            body = {
                {"map_width", 48},
                {"map_height", 48},
                {"min_node_size", 10},
                {"max_depth", 4},
                {"seed", 0}
            };
        }
        std::string response = make_request("POST", "/api/map", body.dump(), true);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to start new game"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse new game response"}};
        }
    }
};

bool has_required_string(const json& args, const std::string& key, std::string& out) {
    if (!args.contains(key) || !args[key].is_string()) {
        return false;
    }
    out = args[key].get<std::string>();
    return true;
}

bool has_required_int(const json& args, const std::string& key, int& out) {
    if (!args.contains(key) || !args[key].is_number_integer()) {
        return false;
    }
    out = args[key].get<int>();
    return true;
}

json validation_error(const std::string& message) {
    return {{"success", false}, {"error", message}};
}

int main() {
    mcp::McpServer server("roguelike-mcp", "1.0.0");
    GameServiceClient client;

    server.register_tool("get_game_state",
        "Get current game state (player HP, position, inventory, game over status)",
        {{"type", "object"}, {"properties", json::object()}},
        [&client](const json&) -> json {
            return client.get_state();
        });

    server.register_tool("new_game",
        "Start a new game with optional map options",
        {{"type", "object"}, {"properties", {
            {"map_width", {{"type", "integer"}, {"minimum", 10}}},
            {"map_height", {{"type", "integer"}, {"minimum", 10}}},
            {"min_node_size", {{"type", "integer"}, {"minimum", 4}}},
            {"max_depth", {{"type", "integer"}, {"minimum", 1}}},
            {"seed", {{"type", "integer"}, {"minimum", 0}}}
        }}},
        [&client](const json& args) -> json {
            return client.new_game(args);
        });

    server.register_tool("move",
        "Move player in specified direction",
        {{"type", "object"}, {"properties", {
            {"direction", {{"type", "string"}, {"enum", {"up", "down", "left", "right"}}}}
        }}, {"required", {"direction"}}},
        [&client](const json& args) -> json {
            std::string dir;
            if (!has_required_string(args, "direction", dir)) {
                return validation_error("Missing or invalid required argument: direction");
            }
            if (dir != "up" && dir != "down" && dir != "left" && dir != "right") {
                return validation_error("direction must be one of: up, down, left, right");
            }
            return client.move(dir);
        });

    server.register_tool("attack",
        "Attack monster at coordinates",
        {{"type", "object"}, {"properties", {
            {"target_x", {{"type", "integer"}, {"minimum", 0}}},
            {"target_y", {{"type", "integer"}, {"minimum", 0}}}
        }}, {"required", {"target_x", "target_y"}}},
        [&client](const json& args) -> json {
            int target_x = 0;
            int target_y = 0;
            if (!has_required_int(args, "target_x", target_x) ||
                !has_required_int(args, "target_y", target_y)) {
                return validation_error("Missing or invalid required arguments: target_x, target_y");
            }
            return client.attack(target_x, target_y);
        });

    server.register_tool("scout_around",
        "Look around: reveal current room (full) + nearby corridors, cache BFS paths to exits",
        {{"type", "object"}, {"properties", {
            {"entry_corridor", {{"type", "integer"}}},
            {"blocked_corridors", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
        }}},
        [&client](const json& args) -> json {
            return client.scout_around(args);
        });

    server.register_tool("use_item",
        "Use item from inventory",
        {{"type", "object"}, {"properties", {
            {"item_id", {{"type", "string"}}},
            {"target", {{"type", "string"}, {"optional", true}}}
        }}, {"required", {"item_id"}}},
        [&client](const json& args) -> json {
            std::string item_id;
            if (!has_required_string(args, "item_id", item_id)) {
                return validation_error("Missing or invalid required argument: item_id");
            }
            return client.use_item(item_id);
        });

    server.register_tool("pickup_item",
        "Pick up loot on current tile (loot is also auto-collected when moving onto it)",
        {{"type", "object"}, {"properties", json::object()}},
        [&client](const json&) -> json {
            json state = client.get_state();
            if (state.contains("error")) return state;
            return {
                {"success", true},
                {"message", "Loot is collected automatically when you move onto its tile. Use move to step on loot."},
                {"state", state}
            };
        });

    server.register_tool("get_available_actions",
        "Get list of possible actions in current state",
        {{"type", "object"}, {"properties", json::object()}},
        [&client](const json&) -> json {
            return client.get_available_actions();
        });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string response = server.process_message(line);
        std::cout << response << std::endl;
    }

    return 0;
}
