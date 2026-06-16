#ifndef GAME_SERVICE_SPECTATOR_HPP
#define GAME_SERVICE_SPECTATOR_HPP

#include <nlohmann/json.hpp>

namespace game {

struct Spectator {
    int x = 0;
    int y = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Spectator, x, y)

}

#endif
