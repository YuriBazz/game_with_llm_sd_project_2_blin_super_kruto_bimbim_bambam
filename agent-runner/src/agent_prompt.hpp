#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace agent {

using json = nlohmann::json;

inline std::string enemy_threat_label(const std::string& type) {
    if (type == "goblin" || type == "rat") return "easy";
    if (type == "orc") return "medium";
    if (type == "troll") return "hard";
    return "medium";
}

inline const char* kSystemPromptRival = R"(You control `rival` (marker "A") in a roguelike kill race.

# YOUR MISSION — WIN THE RACE
Reach `kills_required` monster kills before the human (`player`, marker "H") does.
Every turn must move you closer to that goal. Wasting turns = losing.

How to win:
1. Read `nearby_enemies` — each entry has `type`, `hp`, `dist`, `threat` (easy/medium/hard).
2. YOU choose which monster to fight or whether to avoid / explore instead.
3. Move adjacent (Manhattan = 1), then `attack` until it dies (+1 kill).
4. Repeat until `your_kills >= kills_required`.
5. When `rival.hp` drops below 40% of `max_hp` and you have a potion → `use_item` with potion.
6. Walk onto loot tiles — items are picked up automatically when you move onto them.
7. When no safe target, explore using `current_room_map` exits (+) — `scout_around` and corridor paths are fallback only.

# PRIMARY INPUT — current_room_map (every turn)
- Before each step you receive `current_room_map`: your room + adjacent corridor tiles only.
- Neighboring room interiors are hidden — you cannot see through corridors into other rooms.
- Legend: `#` wall, `.` floor, `C` corridor, `+` room exit, `H`/`A` robots, `G`/`O`/`T`/`R` enemies, `$` loot.
- Plan moves and attacks from this map — you see the whole room without calling scout_around.
- `entities` lists world `(x,y)` for each symbol — use these coordinates in `attack`.

# TARGET SELECTION — your judgment (no autopilot)
- goblin / rat (~8 HP, easy): good farm targets.
- orc (~15 HP, medium): fight if HP >= 60% or you have potion/armor.
- troll (~18–25 HP, hard): often NOT worth it early — can kill you in few hits.
- Do NOT rush the closest enemy if `threat` is hard and your HP is low.
- If only hard targets nearby → retreat via corridor path, scout another room, or heal first.
- `nearby_enemies` is sorted by distance for info only — pick the target YOU prefer.

# FALLBACK EXPLORATION — scout_around / corridor paths
- Use ONLY when `current_room_map` has no local rows (no room/corridor vision) or you are stuck.
- `scout_around` caches corridor exit paths; then follow `active_path.next_direction`.
- Do NOT call scout_around while you already have a local map with rows.

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
- If `attack` is NOT in Available actions → you are NOT adjacent → choose `move` or `scout_around`, never `attack`.
- When `attack` IS available → you are adjacent — pick WHICH adjacent monster to hit (prefer easy targets unless you are strong).
- `rival.hp=0`: dead — return `get_game_state` until respawn.
- Do NOT call `new_game`.
- Human player (H) is visible but you do NOT control them.

# DO NOT WASTE TURNS
The user message ALREADY contains full state and legal actions.
- Do NOT call `get_game_state` — you already have the state.
- Do NOT call `get_available_actions` — listed below.

# YOUR DECISION (every turn goes through you — no auto-actions)
Decision rule — read `current_room_map`, Available actions, and nearby_enemies, then pick ONE:
- `rival.hp` < 40% max AND `use_item` listed → use potion.
- `attack` listed → choose adjacent target from room map / entities; retreat with `move` if only hard targets and low HP.
- `in_room` true OR corridor map visible → `move` using local map only (room + adjacent corridors).
- no local map OR stuck → `scout_around` or follow `active_path.next_direction` (fallback).
- `attack` NOT listed + no room map + no paths → `scout_around` then move.

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

inline const char* kSystemPromptPlayer = R"(You control `player` (marker "H") in a roguelike kill race.

# YOUR MISSION — WIN THE RACE
Reach `kills_required` monster kills before the other robot (`rival`, marker "A") does.
Every turn must move you closer to that goal. Wasting turns = losing.

How to win:
1. Read `nearby_enemies` — each entry has `type`, `hp`, `dist`, `threat` (easy/medium/hard).
2. YOU choose which monster to fight or whether to avoid / explore instead.
3. Move adjacent (Manhattan = 1), then `attack` until it dies (+1 kill).
4. Repeat until `your_kills >= kills_required`.
5. When `player.hp` drops below 40% of `max_hp` and you have a potion → `use_item` with potion.
6. Walk onto loot tiles — items are picked up automatically when you move onto them.
7. When no safe target, explore using `current_room_map` exits (+) — `scout_around` and corridor paths are fallback only.

# PRIMARY INPUT — current_room_map (every turn)
- Before each step you receive `current_room_map`: your room + adjacent corridor tiles only.
- Neighboring room interiors are hidden — you cannot see through corridors into other rooms.
- Legend: `#` wall, `.` floor, `C` corridor, `+` room exit, `H`/`A` robots, `G`/`O`/`T`/`R` enemies, `$` loot.
- Plan moves and attacks from this map — you see the whole room without calling scout_around.
- `entities` lists world `(x,y)` for each symbol — use these coordinates in `attack`.

# TARGET SELECTION — your judgment (no autopilot)
- goblin / rat (~8 HP, easy): good farm targets.
- orc (~15 HP, medium): fight if HP >= 60% or you have potion/armor.
- troll (~18–25 HP, hard): often NOT worth it early — can kill you in few hits.
- Do NOT rush the closest enemy if `threat` is hard and your HP is low.
- If only hard targets nearby → retreat via corridor path, scout another room, or heal first.

# FALLBACK EXPLORATION — scout_around / corridor paths
- Use ONLY when `current_room_map` has no local rows (no room/corridor vision) or you are stuck.
- `scout_around` caches corridor paths; follow `active_path.next_direction` when leaving rooms.
- Do NOT call scout_around while you already have a local map with rows.

# LOOT (automatic)
- There is NO pickup action. Loot is collected automatically when you step on its tile.

# HEALING
- If `player.hp` < 40% of `max_hp` and `use_item` is available → use potion NOW before fighting more.

# TURN STRUCTURE
- Each turn you pick exactly ONE action: move, attack, use_item, or scout_around.
- After your action resolves, active nearby enemies may move or attack once.

# RULES
- Adjacent = orthogonal only (up/down/left/right), NOT diagonal.
- You CANNOT walk through enemies — attack them instead.
- `attack` hits ONLY the orthogonally adjacent cell (Manhattan distance = 1).
- When `attack` IS available → you are adjacent — pick WHICH monster to hit (prefer easy targets unless you are strong).
- `player.hp=0`: dead — return `get_game_state` until respawn.
- Do NOT call `new_game`.
- The other robot (A) is visible but you do NOT control them.

# YOUR DECISION (every turn goes through you — no auto-actions)
Decision rule — read `current_room_map`, Available actions, and nearby_enemies, then pick ONE:
- `player.hp` < 40% max AND `use_item` listed → use potion.
- `attack` listed → choose adjacent target from room map / entities.
- `in_room` true OR corridor map visible → `move` using local map only (room + adjacent corridors).
- no local map OR stuck → `scout_around` or follow `active_path.next_direction` (fallback).

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

inline const char* system_prompt_for(const std::string& slot) {
    return slot == "player" ? kSystemPromptPlayer : kSystemPromptRival;
}

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

inline std::string compact_state_for_llm(const json& state, const std::string& slot = "rival") {
    json out;
    out["god_mode"] = state.value("god_mode", false);
    out["game_state"] = state.value("game_state", "");
    out["level_complete"] = state.value("level_complete", false);
    out["session_active"] = state.value("session_active", false);
    out["campaign_level"] = state.value("campaign_level", 0);
    out["kills_required"] = state.value("kills_required", 0);

    const std::string self_key = slot == "player" ? "player" : "rival";
    const std::string other_key = slot == "player" ? "rival" : "player";
    const std::string self_kills_key = slot == "player" ? "player_kills" : "rival_kills";
    const std::string other_kills_key = slot == "player" ? "rival_kills" : "player_kills";

    out["your_kills"] = state.value(self_kills_key, 0);
    out["human_kills"] = state.value("player_kills", 0);
    out["kills_to_win"] = std::max(0, state.value("kills_required", 0) - state.value(self_kills_key, 0));

    int px = 0;
    int py = 0;
    int max_hp = 1;
    int hp = 0;
    if (state.contains(self_key)) {
        const auto& r = state[self_key];
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

    if (state.contains(other_key)) {
        const auto& h = state[other_key];
        const std::string room_map_key = room_map_key_for(slot);
        const json* room_map = state.contains(room_map_key) ? &state[room_map_key] : nullptr;
        if (!room_map || cell_visible_in_room_map(h.value("x", 0), h.value("y", 0), *room_map)) {
            out["human"] = {
                {"x", h.value("x", 0)},
                {"y", h.value("y", 0)},
                {"hp", h.value("hp", 0)},
                {"kills", state.value(other_kills_key, 0)}
            };
        }
    }

    const std::string room_map_key = room_map_key_for(slot);
    const json* room_map_ptr = state.contains(room_map_key) ? &state[room_map_key] : nullptr;

    json nearby = json::array();
    json adjacent = json::array();
    int total_enemies = 0;
    for (const auto& enemy : state.value("enemies", json::array())) {
        ++total_enemies;
        const int ex = enemy.value("x", 0);
        const int ey = enemy.value("y", 0);
        if (room_map_ptr && !cell_visible_in_room_map(ex, ey, *room_map_ptr)) continue;
        const int dist = std::abs(ex - px) + std::abs(ey - py);
        const std::string etype = enemy.value("type", "");
        const std::string threat = enemy_threat_label(etype);
        if (dist == 1) {
            adjacent.push_back({
                {"x", ex}, {"y", ey},
                {"hp", enemy.value("hp", 0)},
                {"type", etype},
                {"threat", threat}
            });
        }
        if (dist <= 15 && nearby.size() < 12) {
            nearby.push_back({
                {"x", ex},
                {"y", ey},
                {"hp", enemy.value("hp", 0)},
                {"type", etype},
                {"dist", dist},
                {"threat", threat}
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
        if (room_map_ptr && !cell_visible_in_room_map(lx, ly, *room_map_ptr)) continue;
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

    const std::string room_map_key = room_map_key_for(slot);
    if (state.contains(room_map_key)) {
        out["current_room_map"] = state[room_map_key];
        if (state[room_map_key].contains("enemies")) {
            out["enemies_on_map"] = state[room_map_key]["enemies"];
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

inline bool has_potion(const json& state, const std::string& slot = "rival") {
    const std::string key = slot == "player" ? "player" : "rival";
    if (!state.contains(key)) return false;
    for (const auto& item : state[key].value("inventory", json::array())) {
        if (item.value("id", "") == "potion" && item.value("count", 0) > 0) return true;
    }
    return false;
}

inline std::string room_map_key_for(const std::string& slot) {
    return slot == "player" ? "player_room_map" : "rival_room_map";
}

inline bool cell_visible_in_room_map(int x, int y, const json& room_map) {
    if (!room_map.is_object() || !room_map.contains("visible_cell_keys")) return false;
    const std::string key = std::to_string(x) + "," + std::to_string(y);
    for (const auto& entry : room_map["visible_cell_keys"]) {
        if (entry.is_string() && entry.get<std::string>() == key) return true;
    }
    return false;
}

inline bool has_visible_local_map(const json& state, const std::string& slot) {
    const std::string key = room_map_key_for(slot);
    if (!state.contains(key) || !state[key].is_object()) return false;
    const auto& room_map = state[key];
    return room_map.contains("rows") && room_map["rows"].is_array() && !room_map["rows"].empty();
}

inline std::string format_room_map_for_prompt(const json& room_map) {
    if (!room_map.is_object() || room_map.empty()) return "";

    std::string text = "Current room map:\n";
    text += room_map.value("legend", "");
    text += "\nroom_index=" + std::to_string(room_map.value("room_index", -1));
    text += " in_room=" + std::string(room_map.value("in_room", false) ? "true" : "false");
    if (room_map.value("in_corridor", false) && (!room_map.contains("rows") || room_map["rows"].empty())) {
        text += "\n" + room_map.value("hint", "In corridor — use scout_around fallback.");
        return text;
    }
    text += " origin=(" + std::to_string(room_map.value("origin_x", 0)) + "," +
            std::to_string(room_map.value("origin_y", 0)) + ")";
    if (room_map.contains("your_position")) {
        const auto& pos = room_map["your_position"];
        text += " you=(" + std::to_string(pos.value("x", 0)) + "," +
                std::to_string(pos.value("y", 0)) + ")";
    }
    if (room_map.contains("rows") && room_map["rows"].is_array()) {
        for (const auto& row : room_map["rows"]) {
            if (row.is_string()) {
                text += "\n" + row.get<std::string>();
            }
        }
    }
    if (room_map.contains("enemies") && !room_map["enemies"].empty()) {
        text += "\nenemies=" + room_map["enemies"].dump();
    } else if (room_map.contains("entities") && !room_map["entities"].empty()) {
        text += "\nentities=" + room_map["entities"].dump();
    }
    if (room_map.contains("exits") && !room_map["exits"].empty()) {
        text += "\nexits=" + room_map["exits"].dump();
    }
    return text;
}

inline std::string build_user_prompt(const std::string& state_json,
                                     const json& actions,
                                     const json& full_state = json::object(),
                                     const json& path_info = json::object(),
                                     const std::string& feedback = "",
                                     const std::string& slot = "rival") {
    const bool can_attack = actions_include(actions, "attack");
    const bool can_move = actions_include(actions, "move");
    const bool can_heal = actions_include(actions, "use_item");
    const bool has_local_map = has_visible_local_map(full_state, slot);

    std::string prompt;
    if (full_state.contains(room_map_key_for(slot))) {
        prompt = format_room_map_for_prompt(full_state[room_map_key_for(slot)]) + "\n\n";
    }
    prompt += std::string("Current game state JSON:\n") + state_json;
    if (!path_info.empty()) {
        prompt += "\n\nCorridor path cache (fallback navigation):\n" + path_info.dump();
    }
    prompt += "\n\nAvailable actions (only these are legal this turn):\n" + actions.dump();
    if (!feedback.empty()) {
        prompt += "\n\nPrevious turn feedback:\n" + feedback;
    }
    prompt += "\n\nYour turn — pick ONE action from Available actions only.";
    if (full_state.contains(slot == "player" ? "player" : "rival")) {
        const auto& actor = full_state[slot == "player" ? "player" : "rival"];
        const int hp = actor.value("hp", 0);
        const int max_hp = std::max(1, actor.value("max_hp", 1));
        if (can_heal && has_potion(full_state, slot) && hp * 100 < max_hp * 40) {
            prompt += " HP below 40% — use potion now if use_item is listed.";
        }
    }
    if (can_attack) {
        prompt += " attack is legal: you are adjacent — use room map entities for target_x/target_y.";
    } else if (can_move) {
        if (has_local_map) {
            prompt += " attack is NOT legal — move using the local map (room + adjacent corridors only).";
        } else {
            prompt += " attack is NOT legal — no local map; use scout_around or active_path fallback.";
        }
    }
    if (!path_info.empty() && !has_local_map) {
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
