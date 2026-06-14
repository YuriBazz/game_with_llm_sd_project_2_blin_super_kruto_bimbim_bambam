#include "llm_client.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include <curl/curl.h>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

class GameClient {
private:
    std::string base_url_;

public:
    GameClient(const std::string& url) : base_url_(url) {}

    json get_state() {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            std::string url = base_url_ + "/api/state";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        try {
            return json::parse(response);
        } catch (...) {
            std::cerr << "Failed to parse game state response" << std::endl;
            return {{"error", "Failed to parse response"}, {"game_over", false}};
        }
    }

    json move(const std::string& direction) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            std::string url = base_url_ + "/api/move";
            json body = {{"direction", direction}};
            std::string body_str = body.dump();

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body_str.size());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        try {
            return json::parse(response);
        } catch (...) {
            std::cerr << "Failed to parse move response" << std::endl;
            return {{"error", "Move failed"}};
        }
    }
};

int main() {
    int max_steps = std::stoi(std::getenv("MAX_STEPS") ?: "10");
    std::string game_url = std::getenv("GAME_SERVICE_URL") ?: "http://game-service:8080";

    std::cout << "Agent runner starting..." << std::endl;
    std::cout << "Game service URL: " << game_url << std::endl;
    std::cout << "Max steps: " << max_steps << std::endl;

    auto llm = LLMClient::from_env();
    GameClient game(game_url);

    for (int step = 0; step < max_steps; ++step) {
        std::cout << "\n=== Step " << step << " ===" << std::endl;

        json state = game.get_state();
        if (state.contains("error")) {
            std::cerr << "Error getting state: " << state["error"] << std::endl;
            break;
        }

        std::cout << "Player HP: " << state["player"]["hp"] << std::endl;
        std::cout << "Position: [" << state["player"]["x"] << ", " << state["player"]["y"] << "]" << std::endl;

        if (state.value("game_over", false)) {
            std::cout << "Game over!" << std::endl;
            break;
        }

        if (state.value("won", false)) {
            std::cout << "Victory!" << std::endl;
            break;
        }

        // Простая стратегия: чередуем движения
        std::string direction = (step % 4 == 0) ? "right" :
                                (step % 4 == 1) ? "down" :
                                (step % 4 == 2) ? "left" : "up";

        std::cout << "Moving " << direction << std::endl;
        json result = game.move(direction);

        if (result.contains("success") && result["success"]) {
            std::cout << "Move successful!" << std::endl;
        } else {
            std::cout << "Move failed" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\nAgent runner finished" << std::endl;
    return 0;
}
