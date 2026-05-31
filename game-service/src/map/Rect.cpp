//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/map/Rect.hpp>


namespace game {

void to_json(json& j, const Rect& rect) {
    j = json{{"x", rect.x}, {"y", rect.y}, {"w", rect.w}, {"h", rect.h}};
}

void from_json(const json& j, Rect& rect) {
    rect.x = j.value("x", 0);
    rect.y = j.value("y", 0);
    rect.w = j.value("w", 0);
    rect.h = j.value("h", 0);
}

}