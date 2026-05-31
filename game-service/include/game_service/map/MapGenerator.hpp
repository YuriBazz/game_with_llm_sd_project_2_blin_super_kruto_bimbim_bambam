#pragma once

#include <vector>
#include <random>
#include <nlohmann/json.hpp>

#include "LevelMap.hpp"
#include "MapOptions.hpp"
#include "Rect.hpp"
#include "game_service/RandomGenerator.hpp"

using json = nlohmann::json;

namespace game {

class MapGenerator {
    int map_width;
    int map_height;
    int min_node_size;
    int max_depth;
    std::uint64_t seed;

    utils::RandomGenerator& rng;

    struct Node;

    void split_node(Node* node, int depth);
    void generate_rooms(Node* node, std::vector<Rect>& out_rooms);
    Rect get_random_room(Node* node);
    void generate_corridors(Node* node, std::vector<Rect>& out_corridors);

public:
    MapGenerator(utils::RandomGenerator& rng);

    // Теперь возвращает готовую структуру карты, а не JSON
    LevelMap generate_map(MapOptions options);
};

} // namespace game