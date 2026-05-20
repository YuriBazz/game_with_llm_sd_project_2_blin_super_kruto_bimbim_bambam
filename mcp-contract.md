# MCP Contract - Roguelike Game

## Server: roguelike-mcp v1.0.0

### Tools

#### 1. get_game_state
- **Description**: Get current game state
- **Arguments**: none
- **Response**: `{"player": {"hp": 100, "pos": [5,10]}, "game_over": false}`

#### 2. move
- **Description**: Move player in direction
- **Arguments**: `direction` (string, enum: up/down/left/right)
- **Response**: `{"success": true, "new_pos": [5,11]}`

#### 3. attack
- **Description**: Attack monster at coordinates
- **Arguments**: `target_x` (int), `target_y` (int)
- **Response**: `{"success": true, "damage": 15, "target_dead": false}`

#### 4. get_visible_cells
- **Description**: Get visible cells around player
- **Arguments**: `radius` (int, optional, default=5)
- **Response**: `{"cells": [{"x":5,"y":10,"type":"floor"}]}`

#### 5. pickup_item
- **Description**: Pick up item from current cell
- **Arguments**: none
- **Response**: `{"success": true, "item": {"id":"potion","name":"Health Potion"}}`

#### 6. use_item
- **Description**: Use item from inventory
- **Arguments**: `item_id` (string), `target` (string, optional)
- **Response**: `{"success": true, "effect": "heal", "value": 20}`

#### 7. get_available_actions
- **Description**: Get list of possible actions
- **Arguments**: none
- **Response**: `{"actions": ["move", "attack", "use_item", "pickup_item"]}`
