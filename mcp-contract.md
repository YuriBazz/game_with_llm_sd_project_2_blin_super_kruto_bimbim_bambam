# MCP Contract — Roguelike Game

## Server: roguelike-mcp v1.0.0

MCP-tools проксируются через `mcp-server` на HTTP REST API `game-service` (`http://game-service:8080`).

### Общие правила ответов

- **GET `/api/state`** — возвращает объект состояния напрямую (без обёртки).
- **Все остальные игровые эндпоинты** — единый формат:

```json
{
  "success": true,
  "...": "поля, специфичные для действия",
  "state": { "...": "полное состояние игры" }
}
```

- Поле `state` опционально: по умолчанию включено. Чтобы исключить, передайте `?include_state=false` (GET) или query-параметр в URL.
- При ошибке парсинга JSON: HTTP 400, `{"success": false, "error": "Invalid JSON"}`.

### Игровые константы

| Константа | Значение |
|-----------|----------|
| Стартовое HP игрока | 30 |
| Урон игрока | 5 |
| Лечение зелья | 20 |
| Типы тайлов (visible cells) | `"wall"`, `"floor"` |

---

## Схема состояния (`State::to_json`)

**GET `/api/state`** — MCP tool `get_game_state`

```json
{
  "enemies": [
    {"id": 0, "x": 12, "y": 8, "hp": 10, "type": "goblin"}
  ],
  "player": {
    "x": 5,
    "y": 10,
    "hp": 30,
    "max_hp": 30,
    "gold": 0,
    "inventory": []
  },
  "game_over": false,
  "phase": "player_turn",
  "won": false,
  "game_state": "running",
  "steps": 0,
  "map_loot": [
    {"id": 0, "x": 12, "y": 8, "item": {"id": "potion", "name": "Health Potion", "type": "consumable", "count": 1}}
  ]
}
```

| Поле | Тип | Описание |
|------|-----|----------|
| `player.x`, `player.y` | int | Позиция игрока (не массив `pos`) |
| `player.hp`, `player.max_hp` | int | Текущее и максимальное HP |
| `player.gold` | int | Золото |
| `player.inventory` | Item[] | Инвентарь |
| `enemies` | Enemy[] | Враги на карте |
| `game_over` | bool | `true` when game finished |
| `phase` | string | `player_turn` / `victory` / `player_dead` (compat layer) |
| `won` | bool | `true` when player wins |
| `game_state` | string | `running` / `player_victory` / `enemy_victory` |
| `steps` | int | Счётчик ходов |
| `map_loot` | Loot[] | Предметы на карте |

---

## MCP Tools ↔ REST API

### 1. get_game_state

- **REST**: `GET /api/state`
- **Arguments**: none
- **Response**: объект состояния (см. схему выше)

---

### 2. new_game

- **REST**: `POST /api/map`
- **Body**: `MapOptions` (`map_width`, `map_height`, `min_node_size`, `max_depth`, `seed`)
- **Response**:

```json
{
  "success": true,
  "state": { "...": "..." }
}
```

---

### 3. move

- **REST**: `POST /api/move`
- **Body**: `{"direction": "up" | "down" | "left" | "right"}`
- **Response**:

```json
{
  "success": true,
  "x": 5,
  "y": 11,
  "state": { "...": "..." }
}
```

- `success: false` — неверное направление, стена, враг на клетке или не ход игрока. Ход не тратится.

---

### 3. attack

- **REST**: `POST /api/attack`
- **Body**: `{"target_x": 12, "target_y": 8}` — абсолютные координаты цели (должна быть соседней клеткой)
- **Response**:

```json
{
  "success": true,
  "damage": 5,
  "target_dead": false,
  "state": { "...": "..." }
}
```

- `damage` — всегда 5 при успешной атаке.
- `target_dead: true` — враг убит, возможен дроп лута.

---

### 4. get_visible_cells

- **REST**: `GET /api/visible_cells?radius=5`
- **Arguments**: `radius` (int, optional, default=5, min=1, max=10)
- **Response**:

```json
{
  "success": true,
  "cells": [
    {"x": 5, "y": 10, "type": "floor"},
    {"x": 5, "y": 11, "type": "wall"}
  ],
  "state": { "...": "..." }
}
```

- Видимость — круговая область вокруг игрока (`dx² + dy² ≤ radius²`).
- `type` — строка `"wall"` или `"floor"`.

---

### 5. pickup_item

- **REST**: `POST /api/pickup_item`
- **Arguments**: none
- **Response**:

```json
{
  "success": true,
  "items": [
    {"id": "potion", "name": "Health Potion", "type": "consumable", "count": 1}
  ],
  "state": { "...": "..." }
}
```

- `success: false` — на текущей клетке нет лута или не ход игрока.

---

### 6. use_item

- **REST**: `POST /api/use_item`
- **Body**: `{"item_id": "potion", "target": "optional"}`
- **Response**:

```json
{
  "success": true,
  "effect": "heal",
  "value": 20,
  "state": { "...": "..." }
}
```

- Поддерживается `item_id: "potion"` — лечит на 20 HP, если `hp < max_hp`.
- Поле `target` зарезервировано, пока не используется.

---

### 7. get_available_actions

- **REST**: `GET /api/available_actions`
- **Arguments**: none
- **Response**:

```json
{
  "success": true,
  "actions": ["move", "attack", "pickup_item", "use_item"],
  "state": { "...": "..." }
}
```

- Список зависит от фазы (`phase`), соседних клеток, лута под ногами и инвентаря.
- Возможные значения: `move`, `attack`, `pickup_item`, `use_item`.

---

## Дополнительные REST-эндпоинты

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/health` | Healthcheck сервиса |
| POST | `/api/map` | Новая игра. Body: `MapOptions` (`map_width`, `map_height`, `min_node_size`, `max_depth`, `seed`). Response: `{"success": true, "state": {...}}` |
| GET | `/api/map` | Снимок карты (генерируется при `POST /api/map`) |
