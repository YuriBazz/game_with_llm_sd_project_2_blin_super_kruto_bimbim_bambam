#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace agent {

using json = nlohmann::json;

class PathPlanner {
public:
    void reset_session(int session_id) {
        if (session_id_ == session_id) return;
        full_reset(session_id);
    }

    void full_reset(int session_id = -1) {
        session_id_ = session_id;
        room_index_ = -1;
        active_path_index_ = -1;
        last_x_ = -1;
        last_y_ = -1;
        entry_corridor_hint_ = -1;
        last_traveled_corridor_ = -1;
        previous_room_index_ = -1;
        blocked_corridors_.clear();
        recent_rooms_.clear();
        paths_.clear();
        if (session_id >= 0) {
            rng_.seed(static_cast<unsigned>(session_id * 7919 + 104729));
        }
    }

    void clear_paths() {
        active_path_index_ = -1;
        paths_.clear();
    }

    json scout_request_args() const {
        json args = json::object();
        if (entry_corridor_hint_ >= 0) {
            args["entry_corridor"] = entry_corridor_hint_;
        }
        if (!blocked_corridors_.empty()) {
            json blocked = json::array();
            for (int ci : blocked_corridors_) {
                blocked.push_back(ci);
            }
            args["blocked_corridors"] = blocked;
        }
        return args;
    }

    void ingest_scout(const json& scout_result) {
        if (!scout_result.value("success", false)) return;

        const int new_room = scout_result.value("room_index", -1);
        if (room_index_ >= 0 && new_room >= 0 && new_room != room_index_) {
            clear_paths();
        }
        room_index_ = new_room;
        entry_corridor_hint_ = scout_result.value("entry_corridor_index", entry_corridor_hint_);

        paths_.clear();
        active_path_index_ = -1;

        for (const auto& path : scout_result.value("paths", json::array())) {
            CachedPath entry;
            entry.corridor_index = path.value("corridor_index", -1);
            entry.destination_room_index = path.value("destination_room_index", -1);
            entry.goal_x = path.value("goal_x", 0);
            entry.goal_y = path.value("goal_y", 0);
            entry.avoid_return = path.value("avoid_return", false);
            entry.is_forced_exit = path.value("is_forced_exit", false);
            for (const auto& dir : path.value("directions", json::array())) {
                if (dir.is_string()) {
                    entry.directions.push_back(dir.get<std::string>());
                }
            }
            entry.path_length = path.value("length", entry.directions.size());
            if (entry.path_length == 0 && !entry.directions.empty()) {
                entry.path_length = entry.directions.size();
            }
            if (!entry.directions.empty()) {
                paths_.push_back(std::move(entry));
            }
        }

        active_path_index_ = pick_preferred_path_index();
    }

    void on_position_update(int x, int y, int session_id, int room_at_pos) {
        reset_session(session_id);
        if (room_index_ >= 0 && room_at_pos >= 0 && room_at_pos != room_index_) {
            if (last_traveled_corridor_ >= 0) {
                entry_corridor_hint_ = last_traveled_corridor_;
                blocked_corridors_.insert(last_traveled_corridor_);
            }
            if (room_index_ >= 0) {
                previous_room_index_ = room_index_;
                recent_rooms_.push_back(room_index_);
                while (recent_rooms_.size() > 4) {
                    recent_rooms_.pop_front();
                }
            }
            clear_paths();
            room_index_ = room_at_pos;
            last_traveled_corridor_ = -1;
        }
        last_x_ = x;
        last_y_ = y;
    }

    void note_move_executed(bool success) {
        if (!success || active_path_index_ < 0 ||
            active_path_index_ >= static_cast<int>(paths_.size())) {
            return;
        }
        auto& path = paths_[static_cast<size_t>(active_path_index_)];
        if (path.step_index < path.directions.size()) {
            ++path.step_index;
        }
        if (path.step_index >= path.directions.size()) {
            last_traveled_corridor_ = path.corridor_index;
            if (path.avoid_return || path.is_forced_exit) {
                blocked_corridors_.insert(path.corridor_index);
            }
            paths_.erase(paths_.begin() + active_path_index_);
            active_path_index_ = pick_preferred_path_index();
        }
    }

    bool has_paths() const { return !paths_.empty(); }

    bool has_active_step() const {
        if (active_path_index_ < 0 || active_path_index_ >= static_cast<int>(paths_.size())) {
            return false;
        }
        const auto& path = paths_[static_cast<size_t>(active_path_index_)];
        return path.step_index < path.directions.size();
    }

    std::optional<std::string> next_direction() const {
        if (!has_active_step()) return std::nullopt;
        const auto& path = paths_[static_cast<size_t>(active_path_index_)];
        return path.directions[path.step_index];
    }

    json summary_for_prompt() const {
        json out = json::object();
        out["known_paths"] = paths_.size();
        out["room_index"] = room_index_;
        out["entry_corridor_hint"] = entry_corridor_hint_;
        out["previous_room_index"] = previous_room_index_;
        if (!blocked_corridors_.empty()) {
            json blocked = json::array();
            for (int ci : blocked_corridors_) {
                blocked.push_back(ci);
            }
            out["blocked_corridors"] = blocked;
        }
        if (!has_active_step()) {
            out["active_path"] = nullptr;
            return out;
        }
        const auto& path = paths_[static_cast<size_t>(active_path_index_)];
        out["active_path"] = {
            {"corridor_index", path.corridor_index},
            {"goal_x", path.goal_x},
            {"goal_y", path.goal_y},
            {"path_length", path.path_length},
            {"steps_remaining", path.directions.size() - path.step_index},
            {"next_direction", path.directions[path.step_index]},
            {"avoid_return", path.avoid_return},
            {"is_forced_exit", path.is_forced_exit}
        };
        return out;
    }

private:
    bool is_recent_destination_room(int room_index) const {
        if (room_index < 0) return false;
        if (room_index == previous_room_index_) return true;
        for (int recent : recent_rooms_) {
            if (recent == room_index) return true;
        }
        return false;
    }

    struct CachedPath {
        int corridor_index = -1;
        int destination_room_index = -1;
        int goal_x = 0;
        int goal_y = 0;
        size_t path_length = 0;
        bool avoid_return = false;
        bool is_forced_exit = false;
        std::vector<std::string> directions;
        size_t step_index = 0;
    };

    bool is_backtrack_corridor(int corridor_index) const {
        if (corridor_index < 0) return false;
        if (corridor_index == entry_corridor_hint_) return true;
        if (blocked_corridors_.count(corridor_index) > 0) return true;
        return false;
    }

    int pick_random_path_index(bool allow_forced_exit, bool allow_recent_room) const {
        std::vector<int> indices;

        for (size_t i = 0; i < paths_.size(); ++i) {
            const auto& path = paths_[i];
            if (path.avoid_return) continue;
            if (path.is_forced_exit && !allow_forced_exit) continue;
            if (is_backtrack_corridor(path.corridor_index)) continue;
            if (!allow_recent_room && is_recent_destination_room(path.destination_room_index)) {
                continue;
            }
            indices.push_back(static_cast<int>(i));
        }

        if (indices.empty()) return -1;

        std::uniform_int_distribution<size_t> dist(0, indices.size() - 1);
        return indices[dist(rng_)];
    }

    int pick_preferred_path_index() {
        if (paths_.empty()) return -1;

        if (int idx = pick_random_path_index(false, false); idx >= 0) {
            return idx;
        }

        if (int idx = pick_random_path_index(false, true); idx >= 0) {
            return idx;
        }

        if (int idx = pick_random_path_index(true, true); idx >= 0) {
            return idx;
        }

        for (size_t i = 0; i < paths_.size(); ++i) {
            if (!paths_[i].avoid_return) {
                return static_cast<int>(i);
            }
        }

        return 0;
    }

    int session_id_ = -1;
    int room_index_ = -1;
    int active_path_index_ = -1;
    int last_x_ = -1;
    int last_y_ = -1;
    int entry_corridor_hint_ = -1;
    int last_traveled_corridor_ = -1;
    int previous_room_index_ = -1;
    std::unordered_set<int> blocked_corridors_;
    std::deque<int> recent_rooms_;
    std::vector<CachedPath> paths_;
    mutable std::mt19937 rng_{std::random_device{}()};
};

} // namespace agent
