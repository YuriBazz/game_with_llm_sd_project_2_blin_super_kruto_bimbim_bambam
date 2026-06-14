//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/map/MapOptions.hpp>

namespace game {
MapOptions::MapOptions()
: map_width(50), map_height(50), min_node_size(10), max_depth(4), seed(0)
{}

MapOptions::MapOptions(int w, int h, int min_size, int depth, uint32_t custom_seed)
    : map_width(w), map_height(h), min_node_size(min_size), max_depth(depth), seed(custom_seed)
{}

void to_json(json &j, const MapOptions &options) {
    j = json{
                {"map_width", options.map_width},
                {"map_height", options.map_height},
                {"min_node_size", options.min_node_size},
                {"max_depth", options.max_depth},
                {"seed", options.seed}
    };
}

void from_json(const json &j, MapOptions &options) {
    options.map_width = j.value("map_width", 50);
    options.map_height = j.value("map_height", 50);
    options.min_node_size = j.value("min_node_size", 10);
    options.max_depth = j.value("max_depth", 4);
    options.seed = j.value("seed", 0);
}
}