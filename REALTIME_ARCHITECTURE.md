# Real-Time Multi-Agent Game Architecture

## Обзор

Игра переделана с пошагового режима (player turn → enemy turn) на **real-time систему с двумя независимыми агентами**:

- **Player Agent** — управляет героем через API
- **Enemy Agent** — управляет всеми врагами через отдельный API

## Архитектура

### Состояние игры (GameState)

```cpp
enum class GameState {
    Running,          // Игра в процессе
    PlayerVictory,    // Все враги убиты
    EnemyVictory      // Герой убит
};
```

Вместо `Phase::PlayerTurn / Phase::EnemyTurn` используется единое состояние `GameState::Running`. Оба агента действуют параллельно, каждый со своей логикой.

### Условия победы

1. **Player Victory** (`GameState::PlayerVictory`):
   - Все враги убиты (`enemies.empty()`)

2. **Enemy Victory** (`GameState::EnemyVictory`):
   - Герой убит (`player.hp <= 0`)

### Real-Time логика

- Каждый юнит (игрок и враги) имеет **счётчик последнего действия** (`player_last_action_time`, `enemy_last_action_times[i]`)
- **Глобальный счётчик шагов** (`steps`) увеличивается при каждом действии
- Нет блокировки на ход противника — можно совершать действия независимо
- Система готова к расширению с real-time таймерами (стандартное TCP с polling)

## API для Player Agent

### Получить состояние игры
```
GET /api/state
```
Возвращает полное состояние: позиция героя, HP, враги, лут, и т.д.

### Движение героя
```
POST /api/move
Content-Type: application/json

{"direction": "up|down|left|right"}
```

Ответ:
```json
{
  "success": true,
  "x": 15,
  "y": 20,
  "state": { /* full game state */ }
}
```

### Атака героя
```
POST /api/attack
Content-Type: application/json

{"target_x": 15, "target_y": 19}
```

Ответ:
```json
{
  "success": true,
  "damage": 5,
  "target_dead": false,
  "state": { /* full game state */ }
}
```

### Подобрать предмет
```
POST /api/pickup_item
```

Ответ:
```json
{
  "success": true,
  "items": [{"id": "potion", "name": "Health Potion", ...}],
  "state": { /* full game state */ }
}
```

### Использовать предмет
```
POST /api/use_item
Content-Type: application/json

{"item_id": "potion"}
```

### Получить видимые клетки героя
```
GET /api/visible_cells?radius=5
```

Возвращает список клеток в радиусе видимости героя.

### Доступные действия героя
```
GET /api/available_actions
```

## API для Enemy Agent

### Получить список врагов
```
GET /api/enemies
```

Ответ:
```json
{
  "success": true,
  "enemies": [
    {"index": 0, "id": 123, "x": 30, "y": 20, "hp": 10, "type": "goblin"},
    {"index": 1, "id": 124, "x": 35, "y": 25, "hp": 10, "type": "orc"}
  ],
  "state": { /* full game state */ }
}
```

**Важно**: `index` — это индекс в массиве `state.enemies`, используется для всех остальных команд.

### Движение врага
```
POST /api/enemy/move
Content-Type: application/json

{
  "enemy_index": 0,
  "direction": "up|down|left|right"
}
```

Ответ:
```json
{
  "success": true,
  "x": 30,
  "y": 19,
  "state": { /* full game state */ }
}
```

### Атака врага
```
POST /api/enemy/attack
Content-Type: application/json

{
  "enemy_index": 0,
  "target_x": 31,
  "target_y": 20
}
```

Ответ:
```json
{
  "success": true,
  "damage": 2,
  "target_dead": false,
  "state": { /* full game state */ }
}
```

### Видимость врага
```
GET /api/enemy/visible_cells?enemy_index=0&radius=5
```

Возвращает видимые врагом клетки (как для героя, но из точки врага).

## Логика Боя

### Attack Flow для игрока
1. Герой вызывает `/api/attack` с координатами цели
2. Если враг есть на этих координатах:
   - Враг получает урон (`5` из `kPlayerDamage`)
   - Если `target.hp <= 0`:
     - Враг удаляется из `state.enemies`
     - Может выпасть лут (зелье 40%, меч 15%)
     - Герой получает `+10 gold`
   - Проверяются условия победы

### Attack Flow для врага
1. Enemy Agent вызывает `/api/enemy/attack` с координатами цели
2. Если это герой (`target_x == player.x && target_y == player.y`):
   - Герой получает урон (`2` из code)
   - Если `player.hp <= 0`:
     - `state = GameState::EnemyVictory`
3. Враги **не атакуют друг друга** в этой системе

## Пример Loop для LLM Agent

### Player Agent Loop
```python
while True:
    state = requests.get("http://localhost:8080/api/state").json()
    
    if state["state"]["game_state"] != "running":
        print(f"Game Over: {state['state']['game_state']}")
        break
    
    visible = requests.get("http://localhost:8080/api/visible_cells?radius=6").json()
    actions = requests.get("http://localhost:8080/api/available_actions").json()
    
    # LLM decision
    prompt = f"State:\n{state}\nVisible cells:\n{visible}\nAvailable actions:\n{actions}\nWhat's your move?"
    action = llm.chat(prompt)  # e.g. "move down" or "attack at 15,19"
    
    if action.startswith("move"):
        direction = action.split()[-1]
        requests.post("http://localhost:8080/api/move", json={"direction": direction})
    elif action.startswith("attack"):
        # Parse coordinates...
        requests.post("http://localhost:8080/api/attack", json={"target_x": x, "target_y": y})
    # ... etc
```

### Enemy Team Loop
```python
while True:
    state = requests.get("http://localhost:8080/api/state").json()
    
    if state["state"]["game_state"] != "running":
        break
    
    enemies = requests.get("http://localhost:8080/api/enemies").json()["enemies"]
    
    for enemy in enemies:
        idx = enemy["index"]
        visible = requests.get(f"http://localhost:8080/api/enemy/visible_cells?enemy_index={idx}&radius=6").json()
        
        # LLM decision for this enemy
        prompt = f"You are enemy #{idx}.\nState:\n{state}\nYour visible area:\n{visible}\nWhat's your move?"
        action = llm.chat(prompt)
        
        if action.startswith("move"):
            direction = action.split()[-1]
            requests.post("http://localhost:8080/api/enemy/move", 
                         json={"enemy_index": idx, "direction": direction})
        elif action.startswith("attack"):
            requests.post("http://localhost:8080/api/enemy/attack",
                         json={"enemy_index": idx, "target_x": x, "target_y": y})
```

## Различия от старой архитектуры

| Аспект | Старая система | Новая система |
|--------|----------------|---------------|
| **Управление боем** | Phase: PlayerTurn → EnemyTurn | Одновременные независимые действия |
| **Ход врагов** | Chase-AI внутри `process_enemy_turn()` | API endpoints, управляемо извне |
| **Условие победы** | `enemies.empty() && phase == PlayerTurn` | Постоянная проверка в каждом действии |
| **API для врагов** | Нет | Полный набор endpoints |
| **Масштабируемость** | 1 AI loop (ходит по кругу) | Легко подключить 2 независимых LLM агента |

## Развитие системы

### Возможные улучшения

1. **Real-time таймеры**: Вместо счётчика `steps` использовать `std::chrono::system_clock` для синхронизации в миллисекундах
2. **Turn Order Queue**: Упорядоченная очередь ходов всех юнитов с фиксированной скоростью
3. **Fog of War**: Враги не видят героя за стенами
4. **Skill System**: Способности с cooldowns
5. **Item System**: Меч должен работать (сейчас обрабатывается только potion)

## Миграция для Frontend

### web-client может использовать:
- Polling: `GET /api/state` каждые 200ms
- Или WebSocket с подписками (потребует изменения httplib на websocket сервер)

Оба агента (player и enemy team) теперь управляются через HTTP API, идеально для:
- ✅ Двух отдельных LLM агентов
- ✅ MCP integration (мcp-server проксирует эти endpoints)
- ✅ Multi-agent orchestration (как в agent-runner)
- ✅ Real-time визуализации в web-client
