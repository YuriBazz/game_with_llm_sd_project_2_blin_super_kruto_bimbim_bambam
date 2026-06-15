#include "llm_client.hpp"
#include "mcp_client.hpp"

#include <curl/curl.h>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace {

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void log_tool_call(std::ofstream& log, int step, const std::string& tool, const json& args, const json& result) {
    if (!log.is_open()) return;
    log << "[" << timestamp() << "] step=" << step
        << " tool=" << tool
        << " args=" << args.dump()
        << " result_success=" << result.value("success", true)
        << std::endl;
}

bool is_round_finished(const json& state) {
    if (!state.value("session_active", false)) return true;
    if (state.value("game_over", false)) return true;
    if (state.value("level_complete", false)) return true;
    const std::string game_state = state.value("game_state", "running");
    return game_state != "running";
}

bool is_session_running(const json& state) {
    return state.value("session_active", false) &&
           state.value("game_state", "") == "running";
}

void wait_for_start_game(McpClient& mcp, LLMClient& llm, int& last_played_session_id) {
    int wait_seconds = 0;
    while (true) {
        json state = mcp.call_tool("get_game_state");
        const int session_id = state.value("session_id", 0);
        if (is_session_running(state) && session_id != last_played_session_id) {
            std::cout << "Game session started (id=" << session_id << ") — LLM ally joining."
                      << std::endl;
            return;
        }
        if (wait_seconds > 0 && wait_seconds % 120 == 0) {
            llm.ping_ollama_keep_alive();
        }
        if (wait_seconds % 10 == 0) {
            std::cout << "Waiting for human to press Start Game..." << std::endl;
        }
        ++wait_seconds;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace

int main() {
    const int max_steps = std::stoi(std::getenv("MAX_STEPS") ?: "500");
    const char* log_path = std::getenv("AGENT_LOG_PATH");

    std::ofstream log_file;
    if (log_path) {
        log_file.open(log_path, std::ios::trunc);
    }

    std::cout << "Agent runner starting (MCP mode)..." << std::endl;
    std::cout << "Max steps per round: " << max_steps << std::endl;
    std::cout << "Will NOT start a game — waiting for Start Game in browser." << std::endl;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        std::cerr << "curl_global_init failed" << std::endl;
        return 1;
    }

    try {
        auto mcp = McpClient::from_env();
        auto llm = LLMClient::from_env();

        const auto tools = mcp->list_tools();
        std::cout << "MCP tools available: " << tools.size() << std::endl;

        bool urgent_join = false;
        try {
            const json probe = mcp->call_tool("get_game_state");
            urgent_join = is_session_running(probe);
            if (urgent_join) {
                std::cout << "Active game detected — fast-start (skip Ollama practice warmup)."
                          << std::endl;
            }
        } catch (...) {}

        llm->wait_for_provider_ready(urgent_join);

        int last_played_session_id = -1;
        while (true) {
            wait_for_start_game(*mcp, *llm, last_played_session_id);

            json state = mcp->call_tool("get_game_state");
            const int session_id = state.value("session_id", 0);
            llm->reset_session(session_id);
            llm->rewarm_before_game();
            last_played_session_id = session_id;

            for (int step = 0; step < max_steps; ++step) {
                llm->ensure_session(state);
                if (is_round_finished(state)) {
                    std::cout << "Round finished: game_state="
                              << state.value("game_state", "unknown")
                              << ", level_complete=" << state.value("level_complete", false)
                              << std::endl;
                    break;
                }

                std::cout << "\n=== Step " << step << " ===" << std::endl;
                if (state.contains("rival")) {
                    std::cout << "Rival (A) HP: " << state["rival"]["hp"]
                              << " Position: [" << state["rival"]["x"] << ", "
                              << state["rival"]["y"] << "]" << std::endl;
                }
                if (state.contains("player")) {
                    std::cout << "Human (H) at [" << state["player"]["x"] << ", "
                              << state["player"]["y"] << "]" << std::endl;
                }

                json actions = mcp->call_tool("get_available_actions");
                const auto action_list = actions.value("actions", json::array());
                std::cout << "Actions: " << action_list.dump()
                          << " | kills A=" << state.value("rival_kills", 0)
                          << "/" << state.value("kills_required", 0)
                          << " | known_paths=" << llm->path_planner().summary_for_prompt().value("known_paths", 0)
                          << std::endl;

                std::optional<json> decision_opt;
                const int rival_hp = state.contains("rival")
                    ? state["rival"].value("hp", 0)
                    : 0;
                if (rival_hp <= 0) {
                    std::cout << "Rival dead — waiting for respawn..." << std::endl;
                    decision_opt = json{
                        {"tool", "get_game_state"},
                        {"arguments", json::object()}
                    };
                } else {
                    try {
                        decision_opt = llm->decide_action(state, actions);
                    } catch (const std::exception& e) {
                        std::cerr << "decide_action failed: " << e.what()
                                  << " — using mock fallback" << std::endl;
                        decision_opt = llm->decide_with_mock_public(state, actions);
                    }
                }
                if (!decision_opt) {
                    std::cerr << "LLM returned no decision, pausing round." << std::endl;
                    break;
                }
                json decision = *decision_opt;
                try {
                    decision = llm->apply_combat_guard(decision, state, actions);
                    decision = llm->apply_path_guard(decision, state, actions);
                    decision = llm->apply_query_guard(decision, state, actions);
                    decision = llm->apply_stuck_guard(decision, state, actions);
                } catch (const std::exception& e) {
                    std::cerr << "Action guard failed: " << e.what()
                              << " — falling back to scout_around" << std::endl;
                    decision = json{{"tool", "scout_around"}, {"arguments", json::object()}};
                }

                if (!decision.contains("tool") || !decision["tool"].is_string()) {
                    std::cerr << "Invalid LLM decision (missing tool) — using get_game_state"
                              << std::endl;
                    decision = json{{"tool", "get_game_state"}, {"arguments", json::object()}};
                }

                const std::string tool = decision["tool"].get<std::string>();
                json args = decision.value("arguments", json::object());
                if (!args.is_object()) {
                    args = json::object();
                }

                if (tool == "move") {
                    const std::string dir = args.value("direction", "");
                    static const char* kValidDirs[] = {"up", "down", "left", "right"};
                    bool valid_dir = false;
                    for (const char* d : kValidDirs) {
                        if (dir == d) {
                            valid_dir = true;
                            break;
                        }
                    }
                    if (!valid_dir) {
                        std::cerr << "Blocked move with invalid direction '" << dir
                                  << "' — using get_game_state" << std::endl;
                        decision = json{{"tool", "get_game_state"}, {"arguments", json::object()}};
                    }
                }

                const std::string final_tool = decision["tool"].get<std::string>();
                json final_args = decision.value("arguments", json::object());
                if (!final_args.is_object()) {
                    final_args = json::object();
                }
                if (final_tool == "scout_around") {
                    for (const auto& [key, value] :
                         llm->path_planner().scout_request_args().items()) {
                        final_args[key] = value;
                    }
                }
                if (decision.contains("tool") && decision["tool"] == "move") {
                    llm->remember_chosen_direction(
                        final_args.value("direction", ""));
                }

                std::cout << "Tool call: " << final_tool << " " << final_args.dump()
                          << std::endl;

                json result = mcp->call_tool(final_tool, final_args);
                log_tool_call(log_file, step, final_tool, final_args, result);

                if (result.contains("state")) {
                    state = result["state"];
                } else if (final_tool == "get_game_state") {
                    state = result;
                } else {
                    state = mcp->call_tool("get_game_state");
                }

                llm->ensure_session(state);
                llm->note_action_result(final_tool, final_args, result, state);
            }

            std::cout << "Round finished — waiting for next Start Game / reset..." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Agent error: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
}
