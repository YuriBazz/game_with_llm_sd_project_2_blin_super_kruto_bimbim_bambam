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
#include <atomic>

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

void wait_for_game_session(McpClient& mcp, LLMClient& llm, int& last_played_session_id,
                           const std::string& agent_label) {
    int wait_seconds = 0;
    while (true) {
        json state = mcp.call_tool("get_game_state");
        const int session_id = state.value("session_id", 0);
        if (is_session_running(state) && session_id != last_played_session_id) {
            std::cout << "Game session " << session_id << " started — robot "
                      << agent_label << " joining." << std::endl;
            return;
        }
        if (wait_seconds > 0 && wait_seconds % 120 == 0) {
            llm.ping_ollama_keep_alive();
        }
        if (wait_seconds % 10 == 0) {
            std::cout << "Waiting for game session (Start Game / auto-start)..." << std::endl;
        }
        ++wait_seconds;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void wait_for_provider(LLMClient& llm, std::atomic<bool>& provider_ready) {
    while (!provider_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

} // namespace

int main() {
    const int max_steps = std::stoi(std::getenv("MAX_STEPS") ?: "500");
    const char* log_path = std::getenv("AGENT_LOG_PATH");
    const char* agent_slot_env = std::getenv("AGENT_SLOT");
    const std::string agent_slot = agent_slot_env ? agent_slot_env : "rival";
    const char* agent_label_env = std::getenv("AGENT_LABEL");
    const agent::RobotSlotMeta& meta = agent::robot_meta(agent_slot);
    const std::string agent_label = agent_label_env ? agent_label_env : meta.label;

    std::ofstream log_file;
    if (log_path) {
        log_file.open(log_path, std::ios::trunc);
    }

    const std::string provider_key = agent::robot_env_key(agent_slot, "LLM_PROVIDER");
    const std::string model_key = agent::robot_env_key(agent_slot, "MODEL");
    const char* provider_env = agent::robot_env_or_fallback(agent_slot, "LLM_PROVIDER");
    const char* model_env = agent::robot_env_or_fallback(agent_slot, "MODEL");
    const std::string provider_name = provider_env ? provider_env : "mock";
    std::cout << "Agent runner starting (MCP mode, slot=" << agent_slot
              << ", label=" << agent_label << ")..." << std::endl;
    std::cout << "LLM env " << provider_key << "=" << provider_name
              << " " << model_key << "=" << (model_env ? model_env : "(unset)")
              << std::endl;
    std::cout << "Max steps per round: " << max_steps << std::endl;

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
        } catch (...) {}

        std::atomic<bool> provider_ready{false};
        std::thread provider_thread([&llm, urgent_join, &provider_ready]() {
            llm->wait_for_provider_ready(urgent_join);
            provider_ready.store(true);
        });

        int last_played_session_id = -1;
        while (true) {
            wait_for_game_session(*mcp, *llm, last_played_session_id, agent_label);
            wait_for_provider(*llm, provider_ready);

            json state = mcp->call_tool("get_game_state");
            const int session_id = state.value("session_id", 0);
            llm->reset_session(session_id);
            llm->rewarm_before_game();
            last_played_session_id = session_id;

            std::cout << "Robot " << agent_label << " ready — entering kill race."
                      << std::endl;

            for (int step = 0; step < max_steps; ++step) {
                llm->ensure_session(state);
                if (is_round_finished(state)) {
                    std::cout << "Round finished: game_state="
                              << state.value("game_state", "unknown")
                              << ", level_complete=" << state.value("level_complete", false)
                              << std::endl;
                    break;
                }

                std::cout << "\n=== Step " << step << " [" << agent_label << "] ===" << std::endl;
                if (state.contains(agent_slot)) {
                    const auto& self = state[agent_slot];
                    std::cout << "Robot " << agent_label << " HP: " << self.value("hp", 0)
                              << " at [" << self.value("x", 0) << ", "
                              << self.value("y", 0) << "]" << std::endl;
                }

                json actions = mcp->call_tool("get_available_actions");
                const auto action_list = actions.value("actions", json::array());
                const std::string kills_key = meta.kills_key;
                std::cout << "Actions: " << action_list.dump()
                          << " | kills " << agent_label << "=" << state.value(kills_key, 0)
                          << "/" << state.value("kills_required", 0)
                          << " | known_paths=" << llm->path_planner().summary_for_prompt().value("known_paths", 0)
                          << std::endl;

                std::optional<json> decision_opt;
                const int actor_hp = state.contains(agent_slot)
                    ? state[agent_slot].value("hp", 0)
                    : 0;
                if (actor_hp <= 0) {
                    std::cout << "Robot " << agent_label << " dead — waiting for respawn..." << std::endl;
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
                const json agent_view = llm->agent_view(state);
                try {
                    decision = llm->apply_combat_guard(decision, agent_view, actions);
                    decision = llm->apply_path_guard(decision, agent_view, actions);
                    decision = llm->apply_query_guard(decision, agent_view, actions);
                    decision = llm->apply_stuck_guard(decision, agent_view, actions);
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

                std::cout << "[" << agent_label << "] Tool call: " << final_tool << " "
                          << final_args.dump() << std::endl;

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
                llm->note_action_result(
                    final_tool, final_args, result, llm->agent_view(state));
            }

            std::cout << "Round finished — waiting for next game session..." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Agent error: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
}
