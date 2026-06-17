#pragma once

#include "agent_prompt.hpp"
#include "path_planner.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <chrono>
#include <deque>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using json = nlohmann::json;

class LLMClient {
private:
    std::string provider_;
    std::string ollama_url_;
    std::string ollama_model_;
    std::string openai_api_key_;
    std::string openai_model_;
    int max_retries_ = 3;
    int total_tokens_used_ = 0;
    int steps_counter_ = 0;
    int consecutive_query_turns_ = 0;
    int visible_cells_turns_ = 0;
    double temperature_ = 0.3;
    int max_predict_tokens_ = 80;
    std::string agent_slot_ = "rival";
    int max_total_tokens_ = 50000;
    json ollama_prefix_messages_ = json::array();
    bool ollama_prompt_primed_ = false;
    bool ollama_practice_done_ = false;
    bool ollama_prime_attempted_ = false;
    bool ollama_force_cpu_ = false;
    int ollama_num_gpu_ = -1; // -1 = Ollama default, 0 = CPU-only

    static constexpr const char* kOllamaPrimeUser =
        "Confirm readiness. Your mission is to WIN the kill race. "
        "Reply with JSON only: {\"tool\":\"get_game_state\",\"arguments\":{}}";

    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
        output->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    std::optional<std::string> http_get(const std::string& url, long timeout_sec = 30L) const {
        CURL* curl = curl_easy_init();
        if (!curl) return std::nullopt;

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);

        const CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || http_code >= 400) {
            if (http_code >= 400) {
                std::cerr << "HTTP " << http_code << " from GET " << url << std::endl;
            }
            return std::nullopt;
        }
        return response;
    }

    std::optional<std::string> http_post_json(const std::string& url,
                                              const json& body,
                                              const std::vector<std::string>& headers,
                                              long timeout_sec = 30L) const {
        CURL* curl = curl_easy_init();
        if (!curl) return std::nullopt;

        std::string response;
        const std::string payload = body.dump();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);

        struct curl_slist* header_list = nullptr;
        for (const auto& h : headers) {
            header_list = curl_slist_append(header_list, h.c_str());
        }
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        const CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return std::nullopt;
        }
        if (http_code >= 400) {
            std::cerr << "HTTP " << http_code << " from " << url;
            if (!response.empty()) {
                std::cerr << ": " << response.substr(0, 200);
            }
            std::cerr << std::endl;
            return std::nullopt;
        }
        return response;
    }

    static bool ollama_model_matches(const std::string& listed, const std::string& wanted) {
        if (listed == wanted) return true;
        if (listed.rfind(wanted + ":", 0) == 0) return true;
        const auto colon = listed.find(':');
        if (colon != std::string::npos) {
            return listed.substr(0, colon) == wanted;
        }
        return false;
    }

    bool ollama_has_model() const {
        const auto response = http_get(ollama_url_ + "/api/tags", 5L);
        if (!response) return false;

        try {
            const json tags = json::parse(*response);
            for (const auto& model : tags.value("models", json::array())) {
                if (ollama_model_matches(model.value("name", ""), ollama_model_)) {
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    void request_ollama_pull() const {
        std::cout << "Requesting Ollama pull for model: " << ollama_model_ << std::endl;
        const json body = {{"name", ollama_model_}, {"stream", false}};
        const auto response = http_post_json(
            ollama_url_ + "/api/pull",
            body,
            {"Content-Type: application/json"},
            600L
        );
        if (!response) {
            std::cerr << "Ollama pull request failed for model " << ollama_model_ << std::endl;
        }
    }

    json ollama_request_options() const {
        json options = {
            {"temperature", temperature_},
            {"num_predict", max_predict_tokens_},
            {"num_ctx", 4096}
        };
        if (ollama_force_cpu_ || ollama_num_gpu_ == 0) {
            options["num_gpu"] = 0;
        } else if (ollama_num_gpu_ > 0) {
            options["num_gpu"] = ollama_num_gpu_;
        }
        return options;
    }

    std::optional<std::string> ollama_chat(const json& messages, bool allow_cpu_fallback = true) {
        for (int pass = 0; pass < 2; ++pass) {
            const json body = {
                {"model", ollama_model_},
                {"stream", false},
                {"keep_alive", "24h"},
                {"messages", messages},
                {"options", ollama_request_options()}
            };

            const auto response = http_post_json(
                ollama_url_ + "/api/chat",
                body,
                {"Content-Type: application/json"},
                120L
            );
            if (!response) {
                if (pass == 0 && allow_cpu_fallback && !ollama_force_cpu_ && ollama_num_gpu_ != 0) {
                    std::cerr << "Ollama GPU inference failed — retrying on CPU (num_gpu=0). "
                              << "For AMD iGPU use OLLAMA_MODEL=qwen2.5:3b or OLLAMA_NUM_GPU=0."
                              << std::endl;
                    ollama_force_cpu_ = true;
                    ollama_prompt_primed_ = false;
                    ollama_prime_attempted_ = false;
                    std::this_thread::sleep_for(std::chrono::seconds(8));
                    continue;
                }
                return std::nullopt;
            }

            try {
                json parsed = json::parse(*response);
                total_tokens_used_ += parsed.value("eval_count", 0);
                if (parsed.contains("message") && parsed["message"].contains("content")) {
                    return parsed["message"]["content"].get<std::string>();
                }
            } catch (...) {}
            return std::nullopt;
        }
        return std::nullopt;
    }

    const char* system_prompt() const {
        return agent::system_prompt_for(agent_slot_);
    }

    json remap_for_agent_logic(const json& state) const {
        if (agent_slot_ != "player") return state;
        json view = state;
        if (state.contains("player")) view["rival"] = state["player"];
        if (state.contains("rival")) view["player"] = state["rival"];
        view["rival_kills"] = state.value("player_kills", 0);
        view["player_kills"] = state.value("rival_kills", 0);
        if (state.contains("player_room_index")) {
            view["rival_room_index"] = state["player_room_index"];
        }
        if (state.contains("rival_room_index")) {
            view["player_room_index"] = state["rival_room_index"];
        }
        return view;
    }

    void prime_ollama_system_prompt() {
        if (ollama_prompt_primed_) return;
        if (ollama_prime_attempted_ && !ollama_force_cpu_) return;

        ollama_prime_attempted_ = true;
        ollama_prefix_messages_ = json::array({
            json{{"role", "system"}, {"content", system_prompt()}}
        });

        json prime_messages = ollama_prefix_messages_;
        prime_messages.push_back({{"role", "user"}, {"content", kOllamaPrimeUser}});

        std::cout << "Priming Ollama with system prompt (before game)..." << std::endl;
        const auto ack = ollama_chat(prime_messages);
        if (!ack) {
            std::cerr << "Ollama system prompt priming failed — will retry after CPU fallback."
                      << std::endl;
            return;
        }

        ollama_prefix_messages_.push_back({{"role", "user"}, {"content", kOllamaPrimeUser}});
        ollama_prefix_messages_.push_back({{"role", "assistant"}, {"content", *ack}});
        ollama_prompt_primed_ = true;
        std::cout << "Ollama system prompt primed"
                  << (ollama_force_cpu_ ? " (CPU mode)." : ".") << std::endl;
    }

    void run_practice_turn() {
        if (ollama_practice_done_ || provider_ != "ollama") return;
        if (!ollama_prompt_primed_) {
            prime_ollama_system_prompt();
        }
        if (!ollama_prompt_primed_) return;

        const json practice = agent::practice_state();
        const std::string state_json = agent::compact_state_for_llm(practice, agent_slot_);
        const std::string prompt = agent::build_user_prompt(
            state_json,
            agent::practice_actions(),
            practice,
            json::object(),
            "",
            agent_slot_);

        std::cout << "Running practice inference (full game prompt shape)..." << std::endl;
        const auto response = call_ollama(prompt);
        if (response) {
            ollama_practice_done_ = true;
            std::cout << "Ollama practice turn complete — model hot for Step 0." << std::endl;
        } else {
            std::cerr << "Ollama practice turn failed — Step 0 may be slow." << std::endl;
        }
    }

    std::optional<std::string> call_ollama(const std::string& prompt) {
        if (!ollama_prompt_primed_) {
            prime_ollama_system_prompt();
        }

        json messages = json::array({
            json{{"role", "system"}, {"content", system_prompt()}},
            json{{"role", "user"}, {"content", prompt}}
        });

        return ollama_chat(messages);
    }

    std::optional<std::string> call_openai(const std::string& prompt) {
        if (openai_api_key_.empty()) return std::nullopt;

        json body = {
            {"model", openai_model_},
            {"messages", json::array({
                json{{"role", "system"}, {"content", system_prompt()}},
                json{{"role", "user"}, {"content", prompt}}
            })}
        };

        const auto response = http_post_json(
            "https://api.openai.com/v1/chat/completions",
            body,
            {
                "Content-Type: application/json",
                "Authorization: Bearer " + openai_api_key_
            }
        );
        if (!response) return std::nullopt;

        try {
            json parsed = json::parse(*response);
            if (parsed.contains("usage")) {
                total_tokens_used_ += parsed["usage"].value("total_tokens", 0);
            }
            return parsed["choices"][0]["message"]["content"].get<std::string>();
        } catch (...) {}
        return std::nullopt;
    }

    int stuck_turns_ = 0;
    int move_try_index_ = 0;
    int last_ax_ = -1;
    int last_ay_ = -1;
    int block_pos_x_ = -1;
    int block_pos_y_ = -1;
    int repeat_same_move_ = 0;
    std::string last_failed_direction_;
    std::string last_chosen_direction_;
    std::array<bool, 4> blocked_dirs_{};
    std::string action_feedback_;
    std::unordered_map<std::string, std::string> known_tiles_;
    int map_cache_pos_x_ = -1;
    int map_cache_pos_y_ = -1;
    int visible_cells_fetches_here_ = 0;
    int focus_enemy_x_ = -1;
    int focus_enemy_y_ = -1;
    std::deque<std::pair<int, int>> recent_positions_;
    agent::PathPlanner path_planner_;
    int tracked_session_id_ = -1;

    static constexpr const char* kDirs[4] = {"up", "right", "down", "left"};
    static constexpr int kDxy[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    static std::string tile_key(int x, int y) {
        return std::to_string(x) + "," + std::to_string(y);
    }

    int dir_index(const std::string& dir) const {
        for (int i = 0; i < 4; ++i) {
            if (dir == kDirs[i]) return i;
        }
        return -1;
    }

    void sync_blocked_dirs_for_position(int ax, int ay) {
        if (ax != block_pos_x_ || ay != block_pos_y_) {
            blocked_dirs_.fill(false);
            block_pos_x_ = ax;
            block_pos_y_ = ay;
        }
    }

    void mark_direction_blocked(const std::string& dir) {
        const int idx = dir_index(dir);
        if (idx >= 0) blocked_dirs_[static_cast<size_t>(idx)] = true;
    }

    bool is_direction_blocked(const std::string& dir) const {
        const int idx = dir_index(dir);
        return idx >= 0 && blocked_dirs_[static_cast<size_t>(idx)];
    }

    int count_blocked_dirs() const {
        int count = 0;
        for (bool blocked : blocked_dirs_) {
            if (blocked) ++count;
        }
        return count;
    }

    void record_position(int ax, int ay) {
        if (!recent_positions_.empty()) {
            const auto& last = recent_positions_.back();
            if (last.first == ax && last.second == ay) return;
        }
        recent_positions_.push_back({ax, ay});
        while (recent_positions_.size() > 8) {
            recent_positions_.pop_front();
        }
    }

    std::optional<std::pair<int, int>> previous_position() const {
        if (recent_positions_.size() < 2) return std::nullopt;
        return recent_positions_[recent_positions_.size() - 2];
    }

    bool was_recently_at(int x, int y, int within_last = 4) const {
        if (recent_positions_.empty()) return false;
        const int start = static_cast<int>(recent_positions_.size()) - 1;
        const int end = std::max(0, start - within_last);
        for (int i = start - 1; i >= end; --i) {
            if (recent_positions_[static_cast<size_t>(i)].first == x &&
                recent_positions_[static_cast<size_t>(i)].second == y) {
                return true;
            }
        }
        return false;
    }

    bool is_ping_pong() const {
        const size_t n = recent_positions_.size();
        if (n < 4) return false;
        const auto& a = recent_positions_[n - 1];
        const auto& b = recent_positions_[n - 2];
        const auto& c = recent_positions_[n - 3];
        const auto& d = recent_positions_[n - 4];
        return a.first == c.first && a.second == c.second &&
               b.first == d.first && b.second == d.second;
    }

    bool would_backtrack(int nx, int ny) const {
        const auto prev = previous_position();
        return prev.has_value() && prev->first == nx && prev->second == ny;
    }

    bool has_enemy_at(const json& state, int x, int y) const {
        if (!state.contains("enemies")) return false;
        for (const auto& enemy : state["enemies"]) {
            if (enemy["x"].get<int>() == x && enemy["y"].get<int>() == y) return true;
        }
        return false;
    }

    int count_adjacent_enemies(const json& state, int ax, int ay) const {
        int count = 0;
        if (!state.contains("enemies")) return 0;
        for (const auto& enemy : state["enemies"]) {
            const int ex = enemy["x"].get<int>();
            const int ey = enemy["y"].get<int>();
            if (std::abs(ex - ax) + std::abs(ey - ay) == 1) ++count;
        }
        return count;
    }

    bool is_adjacent(int ax, int ay, int tx, int ty) const {
        return std::abs(ax - tx) + std::abs(ay - ty) == 1;
    }

    std::optional<json> pick_attack_decision(const json& state, int ax, int ay) {
        if (!state.contains("enemies")) return std::nullopt;

        if (focus_enemy_x_ >= 0 && focus_enemy_y_ >= 0 &&
            is_adjacent(ax, ay, focus_enemy_x_, focus_enemy_y_) &&
            has_enemy_at(state, focus_enemy_x_, focus_enemy_y_)) {
            return json{
                {"tool", "attack"},
                {"arguments", {{"target_x", focus_enemy_x_}, {"target_y", focus_enemy_y_}}}
            };
        }

        int best_x = -1;
        int best_y = -1;
        int best_hp = 999999;
        for (const auto& enemy : state["enemies"]) {
            const int ex = enemy["x"].get<int>();
            const int ey = enemy["y"].get<int>();
            const int hp = enemy["hp"].get<int>();
            if (!is_adjacent(ax, ay, ex, ey)) continue;
            if (hp < best_hp) {
                best_hp = hp;
                best_x = ex;
                best_y = ey;
            }
        }

        if (best_x < 0) {
            focus_enemy_x_ = -1;
            focus_enemy_y_ = -1;
            return std::nullopt;
        }

        focus_enemy_x_ = best_x;
        focus_enemy_y_ = best_y;
        return json{
            {"tool", "attack"},
            {"arguments", {{"target_x", best_x}, {"target_y", best_y}}}
        };
    }

    bool is_move_candidate(const json& state, int ax, int ay, int nx, int ny,
                           bool allow_backtrack) const {
        if (has_enemy_at(state, nx, ny)) return false;
        if (has_map_cache() && is_blocking_tile(nx, ny)) return false;
        if (!allow_backtrack && would_backtrack(nx, ny)) return false;
        if (is_ping_pong() && was_recently_at(nx, ny, 3)) return false;
        return true;
    }

    int count_move_candidates(const json& state, int ax, int ay, bool allow_backtrack) const {
        int count = 0;
        for (int i = 0; i < 4; ++i) {
            const int nx = ax + kDxy[i][0];
            const int ny = ay + kDxy[i][1];
            if (is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) ++count;
        }
        return count;
    }

    json make_move_decision(const std::string& dir) const {
        return json{{"tool", "move"}, {"arguments", {{"direction", dir}}}};
    }

    std::optional<std::string> direction_from_delta(int dx, int dy) const {
        for (int i = 0; i < 4; ++i) {
            if (kDxy[i][0] == dx && kDxy[i][1] == dy) return kDirs[i];
        }
        return std::nullopt;
    }

    bool has_map_cache() const { return !known_tiles_.empty(); }

    bool is_blocking_tile(int x, int y) const {
        const auto it = known_tiles_.find(tile_key(x, y));
        return it != known_tiles_.end() && it->second == "wall";
    }

    bool is_known_walkable(int x, int y) const {
        const auto it = known_tiles_.find(tile_key(x, y));
        return it != known_tiles_.end() && it->second == "floor";
    }

    void ingest_visible_cells(const json& result) {
        if (!result.contains("cells") || !result["cells"].is_array()) return;
        for (const auto& cell : result["cells"]) {
            if (!cell.contains("x") || !cell.contains("y") || !cell.contains("type")) continue;
            known_tiles_[tile_key(cell["x"].get<int>(), cell["y"].get<int>())] =
                cell["type"].get<std::string>();
        }
    }

    void sync_fetch_counter_for_position(int ax, int ay) {
        if (ax != map_cache_pos_x_ || ay != map_cache_pos_y_) {
            visible_cells_fetches_here_ = 0;
            map_cache_pos_x_ = ax;
            map_cache_pos_y_ = ay;
        }
    }

    std::optional<json> pick_walkable_greedy_move(const json& state, int ax, int ay,
                                                  int tx, int ty) const {
        const bool allow_backtrack = count_move_candidates(state, ax, ay, false) == 0;
        int best_step = -1;
        int best_step_dist = 999999;
        for (int i = 0; i < 4; ++i) {
            const std::string dir = kDirs[i];
            if (is_direction_blocked(dir)) continue;
            const int nx = ax + kDxy[i][0];
            const int ny = ay + kDxy[i][1];
            if (!is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) continue;
            const int dist = std::abs(tx - nx) + std::abs(ty - ny);
            if (dist < best_step_dist) {
                best_step_dist = dist;
                best_step = i;
            }
        }
        if (best_step >= 0) {
            return make_move_decision(kDirs[best_step]);
        }
        return std::nullopt;
    }

    std::optional<json> pick_bfs_move(const json& state, int ax, int ay, int tx, int ty) const {
        if (!has_map_cache()) return std::nullopt;
        if (is_blocking_tile(ax, ay)) return std::nullopt;

        const auto is_goal = [&](int cx, int cy) {
            if (has_enemy_at(state, tx, ty)) {
                return std::abs(cx - tx) + std::abs(cy - ty) == 1 &&
                       !has_enemy_at(state, cx, cy) && is_known_walkable(cx, cy);
            }
            return cx == tx && cy == ty;
        };

        const auto run_bfs = [&](bool avoid_recent) -> std::optional<json> {
            const auto start_key = tile_key(ax, ay);
            std::queue<std::pair<int, int>> q;
            std::unordered_map<std::string, std::pair<int, int>> parent;
            q.push({ax, ay});
            parent[start_key] = {ax, ay};

            while (!q.empty()) {
                const auto [cx, cy] = q.front();
                q.pop();

                if (is_goal(cx, cy)) {
                    int bx = cx;
                    int by = cy;
                    for (int guard = 0; guard < 512; ++guard) {
                        const auto key = tile_key(bx, by);
                        const auto it = parent.find(key);
                        if (it == parent.end()) return std::nullopt;
                        if (it->second.first == ax && it->second.second == ay) {
                            if (auto dir = direction_from_delta(bx - ax, by - ay)) {
                                return make_move_decision(*dir);
                            }
                            return std::nullopt;
                        }
                        bx = it->second.first;
                        by = it->second.second;
                    }
                    return std::nullopt;
                }

                for (int i = 0; i < 4; ++i) {
                    const int nx = cx + kDxy[i][0];
                    const int ny = cy + kDxy[i][1];
                    const auto key = tile_key(nx, ny);
                    if (!is_known_walkable(nx, ny) || parent.count(key)) continue;
                    if (has_enemy_at(state, nx, ny)) continue;
                    if (avoid_recent && was_recently_at(nx, ny, 3)) continue;
                    parent[key] = {cx, cy};
                    q.push({nx, ny});
                }
            }
            return std::nullopt;
        };

        if (auto path = run_bfs(true)) return path;
        if (auto path = run_bfs(false)) return path;
        return pick_walkable_greedy_move(state, ax, ay, tx, ty);
    }

    std::optional<json> pick_explore_move(const json& state, int ax, int ay,
                                          int tx, int ty) const {
        const bool allow_backtrack = count_move_candidates(state, ax, ay, false) == 0;
        int best_step = -1;
        int best_score = -999999;
        for (int i = 0; i < 4; ++i) {
            const std::string dir = kDirs[i];
            if (is_direction_blocked(dir)) continue;
            const int nx = ax + kDxy[i][0];
            const int ny = ay + kDxy[i][1];
            if (!is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) continue;

            int score = 0;
            if (!was_recently_at(nx, ny, 6)) score += 10;
            if (is_ping_pong() && (dir == "up" || dir == "down")) score += 8;
            score -= std::abs(tx - nx) + std::abs(ty - ny);
            if (score > best_score) {
                best_score = score;
                best_step = i;
            }
        }
        if (best_step >= 0) {
            return make_move_decision(kDirs[best_step]);
        }
        return std::nullopt;
    }

    std::optional<json> pick_navigation_move(const json& state, int ax, int ay, int tx, int ty) {
        if (is_ping_pong() || stuck_turns_ >= 2) {
            if (auto explore = pick_explore_move(state, ax, ay, tx, ty)) return explore;
        }
        if (auto bfs = pick_bfs_move(state, ax, ay, tx, ty)) {
            const std::string dir = bfs->value("arguments", json::object()).value("direction", "");
            const int idx = dir_index(dir);
            if (idx >= 0) {
                const int nx = ax + kDxy[idx][0];
                const int ny = ay + kDxy[idx][1];
                const bool allow_backtrack = count_move_candidates(state, ax, ay, false) == 0;
                if (!is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) {
                    if (auto explore = pick_explore_move(state, ax, ay, tx, ty)) return explore;
                } else {
                    return bfs;
                }
            }
        }
        if (auto greedy = pick_walkable_greedy_move(state, ax, ay, tx, ty)) return greedy;
        if (auto explore = pick_explore_move(state, ax, ay, tx, ty)) return explore;
        return pick_rotating_move(state, ax, ay);
    }

    int nearest_enemy_distance(const json& state, int ax, int ay) const {
        const auto [tx, ty] = nearest_enemy_target(state, ax, ay);
        return std::abs(tx - ax) + std::abs(ty - ay);
    }

    bool has_enemies_in_state(const json& state) const {
        return state.contains("enemies") && state["enemies"].is_array() &&
               !state["enemies"].empty();
    }

    std::optional<json> decide_minimal_fallback(const json& view, const json& actions) {
        if (!view.contains("rival")) return std::nullopt;

        const json& pl = view["rival"];
        const int ax = pl["x"].get<int>();
        const int ay = pl["y"].get<int>();
        const int ahp = pl["hp"].get<int>();
        if (ahp <= 0) {
            return json{{"tool", "get_game_state"}, {"arguments", json::object()}};
        }

        const int max_hp = std::max(1, pl["max_hp"].get<int>());
        if (action_available(actions, "use_item") &&
            ahp * 100 < max_hp * 40 &&
            agent::has_potion(view, agent_slot_)) {
            return json{{"tool", "use_item"}, {"arguments", {{"item_id", "potion"}}}};
        }

        if (action_available(actions, "move") && path_planner_.has_active_step()) {
            if (auto dir = path_planner_.next_direction()) {
                return make_move_decision(*dir);
            }
        }

        if (action_available(actions, "scout_around") &&
            !path_planner_.has_paths() &&
            !has_enemies_in_state(view)) {
            return json{{"tool", "scout_around"}, {"arguments", json::object()}};
        }

        if (action_available(actions, "move")) {
            sync_blocked_dirs_for_position(ax, ay);
            if (auto rotated = pick_rotating_move(view, ax, ay)) return *rotated;
        }

        return json{{"tool", "get_game_state"}, {"arguments", json::object()}};
    }

    std::pair<int, int> nearest_enemy_target(const json& state, int ax, int ay) const {
        int tx = ax;
        int ty = ay;
        int best_dist = 999999;
        if (!state.contains("enemies")) return {tx, ty};

        for (const auto& enemy : state["enemies"]) {
            const int ex = enemy["x"].get<int>();
            const int ey = enemy["y"].get<int>();
            const int dist = std::abs(ex - ax) + std::abs(ey - ay);
            if (dist < best_dist) {
                best_dist = dist;
                tx = ex;
                ty = ey;
            }
        }
        return {tx, ty};
    }

    std::optional<json> pick_rotating_move(const json& state, int ax, int ay) {
        const bool allow_backtrack = count_move_candidates(state, ax, ay, false) == 0;
        const int start = is_ping_pong() ? 0 : move_try_index_++;
        for (int attempt = 0; attempt < 4; ++attempt) {
            const int i = (start + attempt) % 4;
            const std::string dir = kDirs[i];
            if (is_direction_blocked(dir)) continue;
            const int nx = ax + kDxy[i][0];
            const int ny = ay + kDxy[i][1];
            if (!is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) continue;
            move_try_index_ = i + 1;
            return make_move_decision(dir);
        }
        return std::nullopt;
    }

    std::optional<json> pick_greedy_move(const json& state, int ax, int ay, int tx, int ty) {
        const bool allow_backtrack = count_move_candidates(state, ax, ay, false) == 0;
        int best_step = -1;
        int best_step_dist = 999999;
        for (int i = 0; i < 4; ++i) {
            const std::string dir = kDirs[i];
            if (is_direction_blocked(dir)) continue;
            const int nx = ax + kDxy[i][0];
            const int ny = ay + kDxy[i][1];
            if (!is_move_candidate(state, ax, ay, nx, ny, allow_backtrack)) continue;
            const int dist = std::abs(tx - nx) + std::abs(ty - ny);
            if (dist < best_step_dist) {
                best_step_dist = dist;
                best_step = i;
            }
        }
        if (best_step >= 0) {
            return make_move_decision(kDirs[best_step]);
        }
        return std::nullopt;
    }

    std::optional<std::string> call_mock(const json& state, const json& actions) {
        if (!state.contains("rival")) {
            return R"({"tool":"get_game_state","arguments":{}})";
        }

        const json& pl = state["rival"];
        const int ax = pl["x"].get<int>();
        const int ay = pl["y"].get<int>();
        const int ahp = pl["hp"].get<int>();
        if (ahp <= 0) {
            return R"({"tool":"get_game_state","arguments":{}})";
        }

        const int max_hp = std::max(1, pl["max_hp"].get<int>());

        if (action_available(actions, "use_item") &&
            ahp * 100 < max_hp * 40 &&
            agent::has_potion(state, agent_slot_)) {
            return R"({"tool":"use_item","arguments":{"item_id":"potion"}})";
        }

        if (action_available(actions, "attack")) {
            if (auto attack = pick_attack_decision(state, ax, ay)) {
                return attack->dump();
            }
        }

        if (action_available(actions, "move") && path_planner_.has_active_step()) {
            if (auto dir = path_planner_.next_direction()) {
                return make_move_decision(*dir).dump();
            }
        }

        if (action_available(actions, "scout_around") &&
            !path_planner_.has_paths() &&
            !has_enemies_in_state(state)) {
            return R"({"tool":"scout_around","arguments":{}})";
        }

        if (action_available(actions, "move")) {
            sync_blocked_dirs_for_position(ax, ay);
            if (auto rotated = pick_rotating_move(state, ax, ay)) {
                return rotated->dump();
            }
        }

        return R"({"tool":"get_game_state","arguments":{}})";
    }

    std::optional<json> normalize_tool_decision(json decision) const {
        if (decision.contains("action") && decision["action"].is_string() && !decision.contains("tool")) {
            decision["tool"] = decision["action"];
        }

        if (!decision.contains("tool")) {
            if (decision.contains("actions") && decision["actions"].is_array()) {
                static const char* kPriority[] = {
                    "attack", "use_item", "move", "scout_around",
                    "get_game_state", "get_available_actions"
                };
                for (const char* preferred : kPriority) {
                    for (const auto& action : decision["actions"]) {
                        if (action.is_string() && action.get<std::string>() == preferred) {
                            decision = json{
                                {"tool", preferred},
                                {"arguments", json::object()}
                            };
                            break;
                        }
                    }
                    if (decision.contains("tool")) break;
                }
            }
            if (!decision.contains("tool") && decision.contains("state") && decision["state"].is_object()) {
                return std::nullopt;
            }
            if (!decision.contains("tool")) {
                return std::nullopt;
            }
        }

        if (!decision.contains("arguments") || decision["arguments"].is_null()) {
            decision["arguments"] = json::object();
        } else if (!decision["arguments"].is_object()) {
            decision["arguments"] = json::object();
        }

        std::string tool = decision["tool"].get<std::string>();
        if (tool == "get_visible_cells") {
            decision["tool"] = "scout_around";
            decision["arguments"] = json::object();
        }

        return decision;
    }

    void repair_move_decision(json& decision, const json& view) const {
        if (!decision.contains("tool") || decision["tool"] != "move") return;
        if (!decision.contains("arguments") || !decision["arguments"].is_object()) {
            decision["arguments"] = json::object();
        }
        json& args = decision["arguments"];
        const std::string dir = args.value("direction", "");
        if (!dir.empty() && dir_index(dir) >= 0) return;
        if (!view.contains("rival")) return;

        const int ax = view["rival"].value("x", 0);
        const int ay = view["rival"].value("y", 0);
        if (args.contains("target_x") && args.contains("target_y")) {
            args["x"] = args["target_x"];
            args["y"] = args["target_y"];
        }
        if (!args.contains("x") || !args.contains("y")) return;

        const int tx = args["x"].get<int>();
        const int ty = args["y"].get<int>();
        const int dx = tx - ax;
        const int dy = ty - ay;
        if (dx == 1 && dy == 0) args["direction"] = "right";
        else if (dx == -1 && dy == 0) args["direction"] = "left";
        else if (dx == 0 && dy == 1) args["direction"] = "down";
        else if (dx == 0 && dy == -1) args["direction"] = "up";
    }

    std::optional<json> salvage_tool_decision(const std::string& raw) const {
        static const std::regex actions_re("\"actions\"\\s*:\\s*\\[([^\\]]*)\\]");
        std::smatch actions_match;
        if (std::regex_search(raw, actions_match, actions_re)) {
            const std::string actions_blob = actions_match[1].str();
            static const char* kPriority[] = {
                "attack", "use_item", "move", "scout_around"
            };
            for (const char* preferred : kPriority) {
                if (actions_blob.find(preferred) != std::string::npos) {
                    return json{
                        {"tool", preferred},
                        {"arguments", json::object()}
                    };
                }
            }
        }

        static const std::regex tool_re("\"tool\"\\s*:\\s*\"([a-z_]+)\"");
        std::smatch tool_match;
        if (!std::regex_search(raw, tool_match, tool_re)) return std::nullopt;

        json decision = json{
            {"tool", tool_match[1].str()},
            {"arguments", json::object()}
        };

        static const std::regex dir_re("\"direction\"\\s*:\\s*\"(up|down|left|right)\"");
        std::smatch dir_match;
        if (std::regex_search(raw, dir_match, dir_re)) {
            decision["arguments"]["direction"] = dir_match[1].str();
            return decision;
        }

        static const std::regex coord_re(
            "\"(?:x|target_x)\"\\s*:\\s*(-?\\d+).{0,40}\"(?:y|target_y)\"\\s*:\\s*(-?\\d+)");
        std::smatch coord_match;
        if (std::regex_search(raw, coord_match, coord_re)) {
            decision["arguments"]["x"] = std::stoi(coord_match[1].str());
            decision["arguments"]["y"] = std::stoi(coord_match[2].str());
            return decision;
        }

        if (decision["tool"] == "scout_around" || decision["tool"] == "get_game_state") {
            return decision;
        }
        return std::nullopt;
    }

    std::optional<json> parse_tool_decision(const std::string& raw) const {
        auto try_parse = [this](const std::string& text) -> std::optional<json> {
            try {
                json decision = json::parse(text);
                return normalize_tool_decision(std::move(decision));
            } catch (...) {}
            return std::nullopt;
        };

        if (auto direct = try_parse(raw)) return direct;

        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '{') continue;
            int depth = 0;
            for (size_t j = i; j < raw.size(); ++j) {
                if (raw[j] == '{') ++depth;
                else if (raw[j] == '}') --depth;
                if (depth == 0) {
                    if (auto parsed = try_parse(raw.substr(i, j - i + 1))) {
                        return parsed;
                    }
                    break;
                }
            }
        }

        std::cerr << "Failed to parse LLM response (first 120 chars): "
                  << raw.substr(0, 120) << std::endl;
        return salvage_tool_decision(raw);
    }

    std::optional<json> decide_with_mock(const json& state, const json& actions) {
        const auto raw = call_mock(remap_for_agent_logic(state), actions);
        if (!raw) return std::nullopt;
        return parse_tool_decision(*raw);
    }

public:
    std::optional<json> decide_with_mock_public(const json& state, const json& actions) {
        return decide_with_mock(state, actions);
    }

private:

    bool action_available(const json& actions, const std::string& name) const {
        for (const auto& action : actions.value("actions", json::array())) {
            if (action.get<std::string>() == name) return true;
        }
        return false;
    }

public:
    LLMClient(const std::string& provider,
              const std::string& ollama_url,
              const std::string& ollama_model,
              const std::string& openai_api_key,
              const std::string& openai_model,
              double temperature,
              int max_predict_tokens,
              int ollama_num_gpu)
        : provider_(provider),
          ollama_url_(ollama_url),
          ollama_model_(ollama_model),
          openai_api_key_(openai_api_key),
          openai_model_(openai_model),
          temperature_(temperature),
          max_predict_tokens_(max_predict_tokens),
          ollama_num_gpu_(ollama_num_gpu),
          ollama_force_cpu_(ollama_num_gpu == 0) {}

    int total_tokens_used() const { return total_tokens_used_; }

    void ping_ollama_keep_alive() {
        if (provider_ != "ollama") return;
        const json body = {
            {"model", ollama_model_},
            {"keep_alive", "24h"},
            {"stream", false},
            {"messages", json::array({
                json{{"role", "user"}, {"content", "."}}
            })},
            {"options", json{{"num_predict", 1}}}
        };
        http_post_json(
            ollama_url_ + "/api/chat",
            body,
            {"Content-Type: application/json"},
            15L
        );
    }

    void rewarm_before_game() {
        // Practice turn at startup already warms the model; skip extra inference here
        // to avoid hammering a fragile GPU stack right before Step 0.
        if (provider_ != "ollama" || !ollama_prompt_primed_) return;
        std::cout << "Model ready for Step 0 (primed at startup)." << std::endl;
    }

    void warmup_ollama() {
        prime_ollama_system_prompt();
        run_practice_turn();
    }

    void wait_for_provider_ready(bool skip_practice_warmup = false) {
        if (provider_ != "ollama") return;

        std::cout << "Waiting for Ollama model " << ollama_model_ << "..." << std::endl;
        bool pull_requested = false;

        for (int attempt = 0; attempt < 300; ++attempt) {
            if (ollama_has_model()) {
                std::cout << "Ollama model ready." << std::endl;
                if (skip_practice_warmup) {
                    prime_ollama_system_prompt();
                } else {
                    warmup_ollama();
                }
                return;
            }

            if (!pull_requested) {
                request_ollama_pull();
                pull_requested = true;
            } else if (attempt % 15 == 0) {
                std::cout << "Still waiting for Ollama model " << ollama_model_
                          << " (attempt " << attempt << ")..." << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cerr << "Ollama model " << ollama_model_
                  << " not ready after wait — will retry per step (no permanent mock fallback)."
                  << std::endl;
    }

    void reset_session(int session_id = -1) {
        stuck_turns_ = 0;
        move_try_index_ = 0;
        last_ax_ = -1;
        last_ay_ = -1;
        block_pos_x_ = -1;
        block_pos_y_ = -1;
        repeat_same_move_ = 0;
        last_failed_direction_.clear();
        last_chosen_direction_.clear();
        blocked_dirs_.fill(false);
        action_feedback_.clear();
        known_tiles_.clear();
        map_cache_pos_x_ = -1;
        map_cache_pos_y_ = -1;
        visible_cells_fetches_here_ = 0;
        focus_enemy_x_ = -1;
        focus_enemy_y_ = -1;
        recent_positions_.clear();
        steps_counter_ = 0;
        consecutive_query_turns_ = 0;
        visible_cells_turns_ = 0;
        tracked_session_id_ = session_id;
        path_planner_.full_reset(session_id);
    }

    void ensure_session(const json& state) {
        const int sid = state.value("session_id", -1);
        if (sid < 0) return;
        if (tracked_session_id_ == sid) return;
        if (tracked_session_id_ >= 0) {
            std::cout << "New game detected (session " << tracked_session_id_
                      << " -> " << sid << ") — clearing path/map cache." << std::endl;
        }
        reset_session(sid);
    }

    agent::PathPlanner& path_planner() { return path_planner_; }
    const agent::PathPlanner& path_planner() const { return path_planner_; }

    void sync_path_planner(const json& state) {
        if (!state.contains("rival")) return;
        const int sid = state.value("session_id", 0);
        const int rx = state["rival"]["x"].get<int>();
        const int ry = state["rival"]["y"].get<int>();
        const int room_at = state.value("rival_room_index", -1);
        path_planner_.on_position_update(rx, ry, sid, room_at);
    }

    void note_action_result(const std::string& tool,
                            const json& args,
                            const json& result,
                            const json& state) {
        action_feedback_.clear();
        if (!state.contains("rival")) return;

        const int ax = state["rival"]["x"].get<int>();
        const int ay = state["rival"]["y"].get<int>();
        sync_blocked_dirs_for_position(ax, ay);
        sync_fetch_counter_for_position(ax, ay);

        if (tool == "move") {
            const bool success = result.value("success", false);
            const std::string dir = args.value("direction", "");
            const bool position_changed = !(last_ax_ == ax && last_ay_ == ay && last_ax_ >= 0);

            if (!success || !position_changed) {
                ++stuck_turns_;
                if (!dir.empty()) {
                    last_failed_direction_ = dir;
                    mark_direction_blocked(dir);
                    if (dir == last_chosen_direction_) {
                        ++repeat_same_move_;
                    }
                    const int idx = dir_index(dir);
                    if (idx >= 0) {
                        const int nx = ax + kDxy[idx][0];
                        const int ny = ay + kDxy[idx][1];
                        if (has_enemy_at(state, nx, ny)) {
                            action_feedback_ =
                                "Enemy blocks that tile — you CANNOT walk through monsters. "
                                "Use `attack` on adjacent enemies; they need multiple hits to die.";
                        } else {
                            action_feedback_ =
                                "Last move direction '" + dir +
                                "' FAILED (wall/blocked or no position change). NEVER repeat it — "
                                "use map data and walk around.";
                        }
                    }
                } else {
                    action_feedback_ =
                        "Last move FAILED. Pick another direction or attack adjacent enemies.";
                }
            } else if (was_recently_at(ax, ay, 3)) {
                ++stuck_turns_;
                action_feedback_ =
                    "You are revisiting the same cells (ping-pong). Do NOT backtrack — try "
                    "up/down or explore a new route.";
            } else if (!is_ping_pong()) {
                stuck_turns_ = 0;
                repeat_same_move_ = 0;
                last_failed_direction_.clear();
                blocked_dirs_.fill(false);
            } else {
                ++stuck_turns_;
            }
        } else if (tool == "attack") {
            if (result.value("target_dead", false)) {
                focus_enemy_x_ = -1;
                focus_enemy_y_ = -1;
            }
        } else if (tool == "scout_around") {
            ingest_visible_cells(result);
            ++visible_cells_fetches_here_;
            path_planner_.ingest_scout(result);
            const int path_count = result.value("paths", json::array()).size();
            const int cell_count = result.value("cells", json::array()).size();
            if (path_count > 0) {
                action_feedback_ =
                    "Scout complete: " + std::to_string(cell_count) + " tiles visible, " +
                    std::to_string(path_count) +
                    " corridor path(s). Follow active_path.next_direction.";
            } else if (cell_count > 0) {
                action_feedback_ =
                    "Scout complete: " + std::to_string(cell_count) +
                    " tiles visible, no corridor exits — move toward enemies or scout again after moving.";
            } else {
                action_feedback_ = "Scout returned nothing useful — try moving first.";
            }
        }

        if (tool == "move") {
            const bool success = result.value("success", false);
            const bool position_changed = !(last_ax_ == ax && last_ay_ == ay && last_ax_ >= 0);
            path_planner_.note_move_executed(success && position_changed);
        }

        sync_path_planner(state);

        record_position(ax, ay);
        last_ax_ = ax;
        last_ay_ = ay;
    }

    void remember_chosen_direction(const std::string& dir) {
        last_chosen_direction_ = dir;
    }

    json apply_stuck_guard(json decision, const json& state, const json& actions) {
        (void)actions;
        if (!state.contains("rival")) return decision;
        if (!decision.contains("tool")) return decision;

        const std::string tool = decision["tool"].get<std::string>();
        if (tool != "move") return decision;

        const int ax = state["rival"]["x"].get<int>();
        const int ay = state["rival"]["y"].get<int>();
        const std::string dir = decision.value("arguments", json::object()).value("direction", "");

        if (dir.empty() || dir_index(dir) < 0) {
            if (auto rotated = pick_rotating_move(state, ax, ay)) return *rotated;
            return decision;
        }

        const int idx = dir_index(dir);
        if (idx >= 0) {
            const int nx = ax + kDxy[idx][0];
            const int ny = ay + kDxy[idx][1];
            if (has_enemy_at(state, nx, ny) || is_direction_blocked(dir)) {
                if (auto rotated = pick_rotating_move(state, ax, ay)) return *rotated;
            }
        }

        if ((repeat_same_move_ >= 2 || stuck_turns_ >= 2 || is_ping_pong()) &&
            is_direction_blocked(dir)) {
            if (auto rotated = pick_rotating_move(state, ax, ay)) return *rotated;
        }

        return decision;
    }

    json apply_combat_guard(json decision, const json& /*state*/, const json& /*actions*/) {
        return decision;
    }

    json apply_path_guard(json decision, const json& state, const json& actions) {
        if (!state.contains("rival") || !decision.contains("tool")) return decision;
        if (!path_planner_.has_active_step()) return decision;

        const auto next_dir = path_planner_.next_direction();
        if (!next_dir) return decision;

        const std::string tool = decision["tool"].get<std::string>();
        if ((tool == "get_game_state" || tool == "get_available_actions") &&
            action_available(actions, "move")) {
            return make_move_decision(*next_dir);
        }

        return decision;
    }

    json apply_query_guard(json decision, const json& state, const json& actions) {
        if (!decision.contains("tool")) return decision;

        std::string tool = decision["tool"].get<std::string>();

        if (tool == "scout_around") {
            decision["arguments"] = path_planner_.scout_request_args();
        } else if (tool == "get_game_state" || tool == "get_available_actions") {
            decision["arguments"] = json::object();
        }

        if (is_query_tool(tool)) {
            ++consecutive_query_turns_;
        } else {
            consecutive_query_turns_ = 0;
        }

        if (consecutive_query_turns_ >= 3 && is_query_tool(tool) && state.contains("rival")) {
            if (auto fallback = decide_minimal_fallback(state, actions)) return *fallback;
        }

        return decision;
    }

    static bool is_query_tool(const std::string& tool) {
        return tool == "get_game_state" ||
               tool == "get_available_actions" ||
               tool == "scout_around";
    }

    bool action_is_available(const json& actions, const std::string& tool) const {
        std::string normalized = tool;
        if (normalized == "get_visible_cells") {
            normalized = "scout_around";
        }
        if (is_query_tool(normalized)) return true;
        for (const auto& action : actions.value("actions", json::array())) {
            if (action.get<std::string>() == normalized) return true;
        }
        return false;
    }

    std::optional<json> decide_action(const json& state, const json& actions) {
        if (total_tokens_used_ >= max_total_tokens_) {
            std::cerr << "Token budget exceeded (" << total_tokens_used_
                      << "/" << max_total_tokens_ << ") — using minimal fallback"
                      << std::endl;
            return decide_minimal_fallback(remap_for_agent_logic(state), actions);
        }

        const json view = remap_for_agent_logic(state);
        ensure_session(state);
        sync_path_planner(view);

        const json path_info = path_planner_.summary_for_prompt();
        const std::string state_json = agent::compact_state_for_llm(state, agent_slot_);

        const int llm_attempts = (provider_ == "ollama") ? 2 : max_retries_;
        for (int attempt = 0; attempt < llm_attempts; ++attempt) {
            const std::string prompt = agent::build_user_prompt(
                state_json, actions, state, path_info, action_feedback_, agent_slot_);

            std::optional<std::string> raw;
            const auto t0 = std::chrono::steady_clock::now();

            if (provider_ == "ollama") {
                raw = call_ollama(prompt);
            } else if (provider_ == "openai") {
                raw = call_openai(prompt);
            } else {
                raw = call_mock(view, actions);
            }

            if (provider_ == "ollama" && raw) {
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                std::cout << "LLM inference: " << ms << "ms" << std::endl;
                if (raw->size() > 4096) {
                    std::cerr << "Warning: oversized LLM response (" << raw->size()
                              << " bytes)" << std::endl;
                }
            }

            if (!raw) {
                std::cerr << "LLM call failed, retry " << (attempt + 1) << std::endl;
                action_feedback_ =
                    "Previous LLM call failed — reply with valid JSON only: "
                    "{\"tool\":\"...\",\"arguments\":{...}}";
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
                continue;
            }

            try {
                if (auto decision = parse_tool_decision(*raw)) {
                    repair_move_decision(*decision, view);
                    const std::string tool = decision->value("tool", "");
                    if (!tool.empty() && !action_is_available(actions, tool)) {
                        std::cerr << "LLM chose unavailable '" << tool
                                  << "' — retry with feedback" << std::endl;
                        action_feedback_ =
                            "Tool '" + tool +
                            "' is NOT in Available actions. Pick exactly one listed action.";
                        continue;
                    }

                    if (tool == "move") {
                        const std::string dir =
                            decision->value("arguments", json::object()).value("direction", "");
                        if (dir.empty() || dir_index(dir) < 0) {
                            std::cerr << "LLM returned invalid move direction"
                                      << " (direction='" << dir << "'"
                                      << ", parsed=" << decision->dump()
                                      << ", raw=" << raw->substr(0, 120) << ")"
                                      << std::endl;
                            action_feedback_ =
                                "Invalid move — use direction up/down/left/right only.";
                            continue;
                        }
                    }

                    action_feedback_.clear();
                    return decision;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse LLM response: " << e.what()
                          << " raw=" << raw->substr(0, 120) << std::endl;
            } catch (...) {
                std::cerr << "Failed to parse LLM response (unknown error) raw="
                          << raw->substr(0, 120) << std::endl;
            }

            action_feedback_ =
                "Could not parse your reply — respond with JSON only, no markdown: "
                "{\"tool\":\"move\",\"arguments\":{\"direction\":\"up\"}}";
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
        }

        if (provider_ != "mock") {
            std::cerr << "LLM decision unusable — using minimal fallback (provider stays "
                      << provider_ << ")" << std::endl;
            return decide_minimal_fallback(view, actions);
        }
        return std::nullopt;
    }

    json agent_view(const json& state) const {
        return remap_for_agent_logic(state);
    }

    static std::unique_ptr<LLMClient> from_env() {
        const char* provider = std::getenv("LLM_PROVIDER");
        const std::string provider_name = provider ? provider : "mock";

        const char* ollama_url = std::getenv("OLLAMA_URL");
        const char* ollama_model = std::getenv("OLLAMA_MODEL");
        const char* openai_key = std::getenv("OPENAI_API_KEY");
        const char* openai_model = std::getenv("OPENAI_MODEL");
        const char* temperature = std::getenv("TEMPERATURE");
        const char* max_tokens = std::getenv("MAX_TOKENS");

        const double temp = temperature ? std::stod(temperature) : 0.3;
        const int env_max = max_tokens ? std::stoi(max_tokens) : 128;
        const int predict = std::max(64, std::min(env_max, 256));
        const char* num_gpu_env = std::getenv("OLLAMA_NUM_GPU");
        int num_gpu = -1;
        if (num_gpu_env && *num_gpu_env) {
            num_gpu = std::stoi(num_gpu_env);
        }

        auto client = std::make_unique<LLMClient>(
            provider_name,
            ollama_url ? ollama_url : "http://ollama:11434",
            ollama_model ? ollama_model : "qwen2.5:7b",
            openai_key ? openai_key : "",
            openai_model ? openai_model : "gpt-4o-mini",
            temp,
            predict,
            num_gpu
        );

        const char* slot = std::getenv("AGENT_SLOT");
        const char* max_total = std::getenv("MAX_TOTAL_TOKENS");
        client->set_agent_config(
            slot ? slot : "rival",
            max_total ? std::stoi(max_total) : 50000);
        return client;
    }

    void set_agent_config(const std::string& slot, int max_total_tokens) {
        agent_slot_ = (slot == "player") ? "player" : "rival";
        max_total_tokens_ = max_total_tokens;
    }

    const std::string& agent_slot() const { return agent_slot_; }
};
