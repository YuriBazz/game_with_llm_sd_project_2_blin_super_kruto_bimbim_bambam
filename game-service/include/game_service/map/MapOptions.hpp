//
// Created by Richard Dzubko on 31.05.2026.
//

#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

namespace game {

using json = nlohmann::json;

struct MapOptions {
    int map_width;
    int map_height;
    int min_node_size;
    int max_depth;
    std::uint64_t seed;

    MapOptions();
    MapOptions(int w, int h, int min_size = 10, int depth = 4, uint32_t custom_seed = 0);
};

void to_json(json& j, const MapOptions& options);
void from_json(const json& j, MapOptions& options);

}
