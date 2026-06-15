#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace agent {

using json = nlohmann::json;

inline const char* kSystemPrompt = R"(You control `rival` (marker "A") in a roguelike kill race.

# YOUR MISSION — WIN THE RACE
Reach `kills_required` monster kills before the human (`player`, marker "H") does.
Every turn must move you closer to that goal. Wasting turns = losing.

How to win:
1. Find monsters in `nearby_enemies` (sorted by distance).
2. Move toward the nearest one until you are adjacent (Manhattan distance = 1).
3. `attack` the same monster repeatedly until it dies (+1 kill for you).
4. Repeat until `your_kills >= kills_required`.
5. When `rival.hp` drops below 40% of `max_hp` and you have a potion → `use_item` with potion.
6. Walk onto loot tiles — items are picked up automatically when you move onto them.
7. When `nearby_enemies` is empty, explore — call `scout_around` or follow cached corridor paths.

# EXPLORATION — scout_around ("оглянуться вокруг")
- One tool for exploration: reveals your whole current room + nearby corridor tiles, and caches corridor exit paths.
- No radius argument — standing in a room always reveals the entire room.
- Scout skips the corridor you entered from when the room has other exits.
- If the room has only one corridor, scout marks it `avoid_return: true` — use once, never backtrack.
- `blocked_corridors` in path cache are dead-end exits — never walk into them again.
- If `known_paths` is 0 and no nearby_enemies → your turn should be `scout_around`.
- After scout, follow `active_path.next_direction` each turn until path completes or room changes.
- Monster adjacent while on a path: if HP >= 40% → `attack`; if HP < 40% → keep following path (retreat).

# LOOT (automatic)
- There is NO pickup action. Loot is collected automatically when you step on its tile.
- Check `nearby_loot` — if loot is on your current tile, your next `move` onto it (or standing on it after a kill drop) adds it to inventory.
- Route toward yellow loot dots when safe; potions and gear help you win the kill race.

# HEALING
- If `rival.hp` < 40% of `max_hp` and `use_item` is available → use potion NOW before fighting more.
- Do not hoard potions while low HP — dying wastes more time than one potion.

# TURN STRUCTURE
- Each turn you pick exactly ONE action: move, attack, use_item, or scout_around.
- scout_around = look around (vision + corridor paths) — counts as your turn.
- After your action resolves, active nearby enemies may move or attack once.
- Enemies do NOT act while you are deciding — only after you commit.
- You only take damage from enemy counter-attacks right after YOUR action.

# RULES
- Adjacent = orthogonal only (up/down/left/right), NOT diagonal.
- You CANNOT walk through enemies — attack them instead.
- Monster HP: goblin ~8 (2 hits), orc ~15 (3 hits), troll ~25 (4+ hits). Your damage ~7.
- `attack` hits ONLY the orthogonally adjacent cell (Manhattan distance = 1). It is legal ONLY when `attack` is listed in Available actions.
- If `attack` is NOT in Available actions → you are NOT adjacent → choose `move`, never `attack`.
- When `attack` IS available → you stand next to a monster → ALWAYS attack, never move away.
- `rival.hp=0`: dead — return `get_game_state` until respawn.
- Do NOT call `new_game`.
- Human player (H) is visible but you do NOT control them.

# DO NOT WASTE TURNS
The user message ALREADY contains full state and legal actions.
- Do NOT call `get_game_state` — you already have the state.
- Do NOT call `get_available_actions` — listed below.

# YOUR DECISION (every turn goes through you — no auto-actions)
Decision rule — read Available actions, then pick ONE:
- `rival.hp` < 40% max AND `use_item` listed → use potion.
- `attack` listed → adjacent monster: attack if HP ok; if HP < 40% and active_path exists → move along path instead.
- `attack` NOT listed + `known_paths` > 0 + active_path.next_direction set → move that direction.
- `attack` NOT listed + `known_paths` == 0 + no nearby_enemies → `scout_around`.
- `attack` NOT listed → `move` toward nearest nearby_enemies or follow path hint.

# OUTPUT — strict JSON only, no markdown, no explanation
{"tool":"<name>","arguments":{...}}

Tools:
- move: {"direction":"up"|"down"|"left"|"right"}
- attack: {"target_x":INT,"target_y":INT}
- use_item: {"item_id":"potion"}
- scout_around: {}
- get_game_state: {}
- get_available_actions: {}
)";

inline json practice_state() {
    return json{
        {"game_state", "running"},
        {"level_complete", false},
        {"session_active", true},
        {"campaign_level", 1},
        {"kills_required", 4},
        {"player_kills", 0},
        {"rival_kills", 0},
        {"rival", {
            {"x", 40}, {"y", 30}, {"hp", 42}, {"max_hp", 42},
            {"respawns_remaining", 2},
            {"inventory", json::array({
                json{{"id", "potion"}, {"count", 1}}
            })}
        }},
        {"player", {{"x", 10}, {"y", 10}, {"hp", 42}, {"max_hp", 42}}},
        {"enemies", json::array({
            json{{"x", 39}, {"y", 30}, {"hp", 8}, {"type", "goblin"}},
            json{{"x", 44}, {"y", 32}, {"hp", 15}, {"type", "orc"}}
        })},
        {"map_loot", json::array()}
    };
}

inline json practice_actions() {
    return json{{"actions", json::array({"move", "attack", "use_item"})}};
}

inline std::string compact_state_for_llm(const json& state) {
    json out;
    out["god_mode"] = state.value("god_mode", false);
    out["game_state"] = state.value("game_state", "");
    out["level_complete"] = state.value("level_complete", false);
    out["session_active"] = state.value("session_active", false);
    out["campaign_level"] = state.value("campaign_level", 0);
    out["kills_required"] = state.value("kills_required", 0);
    out["your_kills"] = state.value("rival_kills", 0);
    out["human_kills"] = state.value("player_kills", 0);
    out["kills_to_win"] = std::max(0, state.value("kills_required", 0) - state.value("rival_kills", 0));

    int px = 0;
    int py = 0;
    int max_hp = 1;
    int hp = 0;
    if (state.contains("rival")) {
        const auto& r = state["rival"];
        px = r.value("x", 0);
        py = r.value("y", 0);
        hp = r.value("hp", 0);
        max_hp = std::max(1, r.value("max_hp", 1));
        out["rival"] = {
            {"x", px},
            {"y", py},
            {"hp", hp},
            {"max_hp", max_hp},
            {"hp_percent", (hp * 100) / max_hp},
            {"respawns_remaining", r.value("respawns_remaining", 0)},
            {"inventory", r.value("inventory", json::array())}
        };
    }

    if (state.contains("player")) {
        const auto& h = state["player"];
        out["human"] = {
            {"x", h.value("x", 0)},
            {"y", h.value("y", 0)},
            {"hp", h.value("hp", 0)},
            {"kills", state.value("player_kills", 0)}
        };
    }

    json nearby = json::array();
    json adjacent = json::array();
    int total_enemies = 0;
    for (const auto& enemy : state.value("enemies", json::array())) {
        ++total_enemies;
        const int ex = enemy.value("x", 0);
        const int ey = enemy.value("y", 0);
        const int dist = std::abs(ex - px) + std::abs(ey - py);
        if (dist == 1) {
            adjacent.push_back({
                {"x", ex}, {"y", ey},
                {"hp", enemy.value("hp", 0)},
                {"type", enemy.value("type", "")}
            });
        }
        if (dist <= 15 && nearby.size() < 12) {
            nearby.push_back({
                {"x", ex},
                {"y", ey},
                {"hp", enemy.value("hp", 0)},
                {"type", enemy.value("type", "")},
                {"dist", dist}
            });
        }
    }
    std::sort(nearby.begin(), nearby.end(), [](const json& a, const json& b) {
        return a.value("dist", 999) < b.value("dist", 999);
    });
    out["adjacent_enemies"] = adjacent;
    out["nearby_enemies"] = nearby;
    out["total_enemies_on_map"] = total_enemies;

    json loot = json::array();
    for (const auto& item : state.value("map_loot", json::array())) {
        const int lx = item.value("x", 0);
        const int ly = item.value("y", 0);
        const int dist = std::abs(lx - px) + std::abs(ly - py);
        if (dist <= 8) {
            json entry = item;
            entry["dist"] = dist;
            entry["auto_pickup"] = true;
            loot.push_back(entry);
        }
    }
    std::sort(loot.begin(), loot.end(), [](const json& a, const json& b) {
        return a.value("dist", 999) < b.value("dist", 999);
    });
    out["nearby_loot"] = loot;
    out["loot_auto_pickup_on_move"] = true;

    if (!nearby.empty() && hp > 0) {
        const int ex = nearby[0].value("x", px);
        const int ey = nearby[0].value("y", py);
        const int dx = ex - px;
        const int dy = ey - py;
        std::string hint;
        if (std::abs(dx) + std::abs(dy) == 1) {
            hint = "attack";
        } else if (std::abs(dx) >= std::abs(dy) && dx != 0) {
            hint = dx > 0 ? "right" : "left";
        } else if (dy != 0) {
            hint = dy > 0 ? "down" : "up";
        }
        if (!hint.empty()) {
            out["suggested_action"] = hint;
            out["nearest_enemy"] = {{"x", ex}, {"y", ey}, {"dist", nearby[0].value("dist", 0)}};
        }
    }

    return out.dump();
}

inline bool actions_include(const json& actions, const std::string& name) {
    for (const auto& action : actions.value("actions", json::array())) {
        if (action.get<std::string>() == name) return true;
    }
    return false;
}

inline bool has_potion(const json& state) {
    if (!state.contains("rival")) return false;
    for (const auto& item : state["rival"].value("inventory", json::array())) {
        if (item.value("id", "") == "potion" && item.value("count", 0) > 0) return true;
    }
    return false;
}

inline std::string build_user_prompt(const std::string& state_json,
                                     const json& actions,
                                     const json& full_state = json::object(),
                                     const json& path_info = json::object(),
                                     const std::string& feedback = "") {
    const bool can_attack = actions_include(actions, "attack");
    const bool can_move = actions_include(actions, "move");
    const bool can_heal = actions_include(actions, "use_item");

    std::string prompt = std::string(
        "Current game state JSON:\n") + state_json;
    if (!path_info.empty()) {
        prompt += "\n\nCorridor path cache:\n" + path_info.dump();
    }
    prompt += "\n\nAvailable actions (only these are legal this turn):\n" + actions.dump();
    if (!feedback.empty()) {
        prompt += "\n\nPrevious turn feedback:\n" + feedback;
    }
    prompt += "\n\nYour turn — pick ONE action from Available actions only.";
    if (full_state.contains("rival")) {
        const int hp = full_state["rival"].value("hp", 0);
        const int max_hp = std::max(1, full_state["rival"].value("max_hp", 1));
        if (can_heal && has_potion(full_state) && hp * 100 < max_hp * 40) {
            prompt += " HP below 40% — use potion now if use_item is listed.";
        }
    }
    if (can_attack) {
        prompt += " attack is legal: you are adjacent to a monster — attack it now.";
    } else if (can_move) {
        prompt += " attack is NOT legal (not adjacent) — choose move toward nearest nearby_enemies or explore.";
        try {
            const json compact = json::parse(state_json);
            if (compact.contains("suggested_action") &&
                compact["suggested_action"].is_string()) {
                const std::string hint = compact["suggested_action"].get<std::string>();
                if (hint != "attack") {
                    prompt += " suggested move toward nearest enemy: " + hint + ".";
                }
            }
        } catch (...) {}
    }
    if (!path_info.empty()) {
        const int known = path_info.value("known_paths", 0);
        if (known == 0 && !can_attack && can_move) {
            prompt += " No cached corridor paths — call scout_around before wandering.";
        } else if (path_info.contains("active_path") && !path_info["active_path"].is_null()) {
            const auto& ap = path_info["active_path"];
            if (ap.contains("next_direction")) {
                prompt += " Follow active_path.next_direction: " +
                          ap["next_direction"].get<std::string>() + ".";
            }
        }
    }
    prompt += " JSON only.";
    return prompt;
}

} // namespace agent
