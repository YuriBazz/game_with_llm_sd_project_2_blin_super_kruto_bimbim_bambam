# Real-Time Multi-Agent Game - Запуск

## Требования

- C++17+ компилятор (clang/g++)
- CMake 3.16+
- Python 3.8+ (для примеров агентов)
- requests library: `pip install requests`

## Сборка game-service

```bash
cd game-service
cmake -B build
cmake --build build
```

Исполняемый файл: `build/game_service`

## Запуск сервера

```bash
./game-service/build/game_service
```

Сервис запустится на `http://0.0.0.0:8080`

## Запуск примеров агентов

### Терминал 1: Запустить игровой сервис
```bash
./game-service/build/game_service
```

### Терминал 2: Запустить Player Agent
```bash
python3 examples/player_agent_example.py
```

### Терминал 3: Запустить Enemy Team Agent
```bash
python3 examples/enemy_team_agent_example.py
```

## Как это работает

1. **game-service** запускается на порту 8080
2. **Player Agent** (LLM) подключается и управляет героем:
   - `GET /api/state` — получить состояние
   - `POST /api/move` — движение
   - `POST /api/attack` — атака
3. **Enemy Team Agent** (LLM) подключается и управляет врагами:
   - `GET /api/enemies` — список врагов
   - `POST /api/enemy/move` — движение врага
   - `POST /api/enemy/attack` — атака врага

Оба агента действуют **параллельно и независимо**, отправляя команды через HTTP API.

## API Endpoints

### Для Player (Героя)

| Метод | Endpoint | Описание |
|-------|----------|---------|
| GET | `/api/state` | Полное состояние игры |
| GET | `/api/map` | Текущая карта |
| GET | `/api/visible_cells?radius=6` | Видимые герою клетки |
| GET | `/api/available_actions` | Доступные действия |
| POST | `/api/move` | Движение (`{"direction": "up\|down\|left\|right"}`) |
| POST | `/api/attack` | Атака (`{"target_x": <int>, "target_y": <int>}`) |
| POST | `/api/pickup_item` | Подобрать предмет |
| POST | `/api/use_item` | Использовать предмет (`{"item_id": <string>}`) |

### Для Enemy Team (Врагов)

| Метод | Endpoint | Описание |
|-------|----------|---------|
| GET | `/api/enemies` | Список врагов с индексами |
| GET | `/api/enemy/visible_cells?enemy_index=0&radius=6` | Видимые врагу клетки |
| POST | `/api/enemy/move` | Движение врага (`{"enemy_index": 0, "direction": "up\|down\|left\|right"}`) |
| POST | `/api/enemy/attack` | Атака врага (`{"enemy_index": 0, "target_x": <int>, "target_y": <int>}`) |

### Управление игрой

| Метод | Endpoint | Описание |
|-------|----------|---------|
| POST | `/api/map` | Запустить новую игру (`{"seed": 42, "room_count": 5, ...}`) |
| GET | `/health` | Проверка здоровья сервиса |

## Пример запуска в curl

```bash
# 1. Запустить новую игру
curl -X POST http://localhost:8080/api/map \
  -H "Content-Type: application/json" \
  -d '{"seed": 42, "room_count": 5, "room_max_size": 12, "room_min_size": 6}'

# 2. Получить состояние
curl http://localhost:8080/api/state

# 3. Герой движется вверх
curl -X POST http://localhost:8080/api/move \
  -H "Content-Type: application/json" \
  -d '{"direction": "up"}'

# 4. Получить врагов
curl http://localhost:8080/api/enemies

# 5. Первый враг движется вправо
curl -X POST http://localhost:8080/api/enemy/move \
  -H "Content-Type: application/json" \
  -d '{"enemy_index": 0, "direction": "right"}'
```

## Структура ответов

Все ответы включают полное состояние (если `include_state=true`):

```json
{
  "success": true,
  "x": 15,
  "y": 20,
  "state": {
    "player": {
      "x": 15,
      "y": 20,
      "hp": 30,
      "max_hp": 30,
      "gold": 0,
      "inventory": [...]
    },
    "enemies": [...],
    "game_state": "running",
    "steps": 42,
    "map_loot": [...]
  }
}
```

## Условия победы/поражения

### Player Victory (герой побеждает)
- `game_state == "player_victory"`
- Все враги убиты (`enemies.empty()`)

### Enemy Victory (враги побеждают)
- `game_state == "enemy_victory"`
- Герой убит (`player.hp <= 0`)

## Интеграция с LLM

Оба агента могут быть подключены к LLM через:

1. **Direct Integration**: Каждый агент запрашивает LLM в loop'е
2. **MCP Server**: Обернуть endpoints в MCP tools
3. **Agent Framework**: Использовать LangChain, CrewAI и т.д.

Пример с pseudocode:

```python
while game_running:
    state = get_game_state()
    visible = get_visible_cells()
    
    prompt = f"Current state: {state}. Visible: {visible}. What's your move?"
    decision = llm.chat(prompt)  # Например, ollama или OpenAI
    
    execute_action(decision)
```

## Troubleshooting

### Port 8080 already in use
```bash
lsof -i :8080
kill -9 <PID>
```

### CMake errors
```bash
cd game-service
rm -rf build CMakeCache.txt
cmake -B build
cmake --build build
```

### Compiler not found
```bash
# macOS
brew install llvm

# Ubuntu/Debian
sudo apt-get install build-essential cmake
```

## Дальнейшее развитие

1. **WebSocket** для real-time notifications
2. **Turn Queue** система с фиксированной скоростью
3. **Skill System** с cooldowns
4. **Multi-level Dungeon** с порталами между уровнями
5. **Item System** расширение (мечи, броня, зелья боевых эффектов)
