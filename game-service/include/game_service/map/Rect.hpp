//
// Created by Richard Dzubko on 31.05.2026.
//

#ifndef GAME_SERVICE_RECT_HPP
#define GAME_SERVICE_RECT_HPP

#include <nlohmann/json.hpp>

namespace game {

using json = nlohmann::json;

struct Rect {
    int x, y, w, h;
};

void to_json(json& j, const Rect& rect);
void from_json(const json& j, Rect& rect);

}

#endif //GAME_SERVICE_RECT_HPP