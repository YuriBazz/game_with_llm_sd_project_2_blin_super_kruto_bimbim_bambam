#pragma once

#include <string>
#include <optional>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class LLMClient {
private:
    bool use_mock_ = false;

public:
    LLMClient(bool use_mock = true) : use_mock_(use_mock) {
        if (use_mock_) {
            std::cout << "LLM Client initialized in MOCK mode" << std::endl;
        }
    }

    std::optional<std::string> chat(const std::string& prompt, int max_tokens = 150) {
        if (use_mock_) {
            return "{\"action\": \"move\", \"direction\": \"right\"}";
        }
        return std::nullopt;
    }

    static std::unique_ptr<LLMClient> from_env() {
        const char* provider = std::getenv("LLM_PROVIDER");
        if (!provider || std::string(provider) == "mock") {
            return std::make_unique<LLMClient>(true);
        }
        return std::make_unique<LLMClient>(false);
    }
};
