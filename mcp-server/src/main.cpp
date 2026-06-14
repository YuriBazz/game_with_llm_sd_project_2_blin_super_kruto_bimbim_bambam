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
        
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }
        }
        
        if (is_json) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return "";
        }
        return response;
    }
    
public:
    GameServiceClient(const std::string& host = "localhost", int port = 8080) 
        : host_(host), port_(port) {
        // Allow override via environment variables
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
        json body = {{"direction", direction}};
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
        json body = {{"target_x", target_x}, {"target_y", target_y}};
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
        std::string path = "/api/visible_cells?radius=" + std::to_string(radius);
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
    
    json pickup_item() {
        std::string response = make_request("POST", "/api/pickup_item", "", true);
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to pickup item"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse pickup response"}};
        }
    }
    
    json use_item(const std::string& item_id) {
        json body = {{"item_id", item_id}};
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
    
    json get_available_actions() {
        std::string response = make_request("GET", "/api/available_actions");
        if (response.empty()) {
            return {{"success", false}, {"error", "Failed to get available actions"}};
        }
        try {
            return json::parse(response);
        } catch (...) {
            return {{"success", false}, {"error", "Failed to parse available actions response"}};
        }
    }
};

int main() {
    mcp::McpServer server("roguelike-mcp", "1.0.0");
    GameServiceClient client;

    server.register_tool("get_game_state",
        "Get current game state (player HP, position, inventory, game over status)",
        {{"type", "object"}, {"properties", json::object()}},
        [&client](const json&) -> json {
            return client.get_state();
        });

    server.register_tool("move",
        "Move player in specified direction",
        {{"type", "object"}, {"properties", {
            {"direction", {{"type", "string"}, {"enum", {"up", "down", "left", "right"}}}}
        }}, {"required", {"direction"}}},
        [&client](const json& args) -> json {
            std::string dir = args["direction"];
            return client.move(dir);
        });

    server.register_tool("attack",
        "Attack monster at coordinates",
        {{"type", "object"}, {"properties", {
            {"target_x", {{"type", "integer"}, {"minimum", 0}, {"maximum", 19}}},
            {"target_y", {{"type", "integer"}, {"minimum", 0}, {"maximum", 19}}}
        }}, {"required", {"target_x", "target_y"}}},
        [&client](const json& args) -> json {
            int target_x = args["target_x"];
            int target_y = args["target_y"];
            return client.attack(target_x, target_y);
        });

    server.register_tool("get_visible_cells",
        "Get visible cells within radius from player",
        {{"type", "object"}, {"properties", {
            {"radius", {{"type", "integer"}, {"default", 5}, {"minimum", 1}, {"maximum", 10}}}
        }}},
        [&client](const json& args) -> json {
            int radius = args.value("radius", 5);
            return client.get_visible_cells(radius);
        });

    server.register_tool("pickup_item",
        "Pick up item from current cell",
        {{"type", "object"}, {"properties", json::object()}},
        [&client](const json&) -> json {
            return client.pickup_item();
        });

    server.register_tool("use_item",
        "Use item from inventory",
        {{"type", "object"}, {"properties", {
            {"item_id", {{"type", "string"}}},
            {"target", {{"type", "string"}, {"optional", true}}}
        }}, {"required", {"item_id"}}},
        [&client](const json& args) -> json {
            std::string item_id = args["item_id"];
            return client.use_item(item_id);
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
