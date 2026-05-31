//
// Created by Richard Dzubko on 31.05.2026.
//

#include <game_service/map/MapGenerator.hpp>

#include <algorithm>
#include <chrono>

namespace game {

struct MapGenerator::Node {
    Rect block;
    Rect room;
    Node* left = nullptr;
    Node* right = nullptr;

    explicit Node(const Rect r) : block(r), room()
    {}

    ~Node() {
        delete left;
        delete right;
    }

    [[nodiscard]] bool is_leaf() const {
        return left == nullptr && right == nullptr;
    }
};

void MapGenerator::split_node(Node *node, int depth) {
    if (depth >= max_depth) return;

    bool split_h = rng.get_random(0, 1) == 0;
    if (node->block.w > node->block.h * 1.5) split_h = false;
    else if (node->block.h > node->block.w * 1.5) split_h = true;

    const int max_split = (split_h ? node->block.h : node->block.w) - min_node_size;

    if (max_split <= min_node_size) return;

    const int split_pos = rng.get_random(min_node_size, max_split);

    if (split_h) {
        node->left = new Node({node->block.x, node->block.y, node->block.w, split_pos});
        node->right = new Node({node->block.x, node->block.y + split_pos, node->block.w, node->block.h - split_pos});
    } else {
        node->left = new Node({node->block.x, node->block.y, split_pos, node->block.h});
        node->right = new Node({node->block.x + split_pos, node->block.y, node->block.w - split_pos, node->block.h});
    }

    split_node(node->left, depth + 1);
    split_node(node->right, depth + 1);
}

void MapGenerator::generate_rooms(Node *node, std::vector<Rect> &out_rooms) {
    if (!node) return;

    if (node->is_leaf()) {
        int min_w = std::max(4, node->block.w - 4);
        int min_h = std::max(4, node->block.h - 4);

        node->room.w = rng.get_random(min_w, node->block.w - 2);
        node->room.h = rng.get_random(min_h, node->block.h - 2);
        node->room.x = node->block.x + rng.get_random(1, node->block.w - node->room.w - 1);
        node->room.y = node->block.y + rng.get_random(1, node->block.h - node->room.h - 1);

        out_rooms.push_back(node->room);
    } else {
        generate_rooms(node->left, out_rooms);
        generate_rooms(node->right, out_rooms);
    }
}

Rect MapGenerator::get_random_room(Node *node) {
    if (node->is_leaf()) return node->room;

    if (node->left && node->right) {
        return (rng.get_random(0, 1) == 0) ? get_random_room(node->left) : get_random_room(node->right);
    }
    if (node->left) return get_random_room(node->left);
    if (node->right) return get_random_room(node->right);

    return {0, 0, 0, 0};
}

void MapGenerator::generate_corridors(Node *node, std::vector<Rect> &out_corridors) {
    if (!node || node->is_leaf()) return;

    generate_corridors(node->left, out_corridors);
    generate_corridors(node->right, out_corridors);

    if (node->left && node->right) {
        Rect roomA = get_random_room(node->left);
        Rect roomB = get_random_room(node->right);

        if (roomA.w == 0 || roomB.w == 0) return;

        int xA = roomA.x + roomA.w / 2;
        int yA = roomA.y + roomA.h / 2;
        int xB = roomB.x + roomB.w / 2;
        int yB = roomB.y + roomB.h / 2;

        if (rng.get_random(0, 1) == 0) {
            out_corridors.push_back({std::min(xA, xB), yA, std::abs(xA - xB) + 1, 1});
            out_corridors.push_back({xB, std::min(yA, yB), 1, std::abs(yA - yB) + 1});
        } else {
            out_corridors.push_back({xA, std::min(yA, yB), 1, std::abs(yA - yB) + 1});
            out_corridors.push_back({std::min(xA, xB), yB, std::abs(xA - xB) + 1, 1});
        }
    }
}

MapGenerator::MapGenerator(utils::RandomGenerator& rng)
    : map_width(0), map_height(0), min_node_size(0), max_depth(0), seed(0), rng(rng) {
    rng.seed(seed);
}

LevelMap MapGenerator::generate_map(MapOptions options) {
    this->map_width = options.map_width;
    this->map_height = options.map_height;
    this->min_node_size = options.min_node_size;
    this->max_depth = options.max_depth;

    if (options.seed == 0) {
        const auto p1 = std::chrono::system_clock::now();
        this->seed = std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count();
    } else {
        this->seed = options.seed;
    }
    this->rng.seed(this->seed);

    const auto root = new Node({1, 1, map_width - 2, map_height - 2});
    split_node(root, 0);

    LevelMap result;
    result.width = map_width;
    result.height = map_height;
    result.seed = seed;

    generate_rooms(root, result.rooms);
    generate_corridors(root, result.corridors);

    delete root;

    // Инициализация сетки сплошными стенами
    result.grid.assign(map_width * map_height, TileType::Wall);

    // Вырезаем комнаты в сетке
    for (const auto& r : result.rooms) {
        for (int y = r.y; y < r.y + r.h; ++y) {
            for (int x = r.x; x < r.x + r.w; ++x) {
                // Защита от выхода за пределы массива
                if (x >= 0 && x < map_width && y >= 0 && y < map_height) {
                    result.grid[y * map_width + x] = TileType::Floor;
                }
            }
        }
    }

    // Вырезаем коридоры в сетке
    for (const auto& c : result.corridors) {
        for (int y = c.y; y < c.y + c.h; ++y) {
            for (int x = c.x; x < c.x + c.w; ++x) {
                if (x >= 0 && x < map_width && y >= 0 && y < map_height) {
                    result.grid[y * map_width + x] = TileType::Floor;
                }
            }
        }
    }



    return result;
}

} // namespace game