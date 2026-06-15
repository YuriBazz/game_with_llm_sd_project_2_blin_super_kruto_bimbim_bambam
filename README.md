# game_with_llm_sd_project_2_blin_super_kruto_bimbim_bambam

Roguelike с LLM-агентами: C++ game-service, MCP-сервер, agent-runner, web-клиент. Real-time kill race по уровням подземелья.

## Концепт игры

Две языковые модели оказались заперты в процедурно сгенерированном подземелье, населённом гоблинами, троллями и загадочными орками (последние, по слухам, просто затесались в enum и никто не помнит, зачем).

Изначально соперники хотели разрешить конфликт по-взрослому — в октагоне. Октагона в данже не оказалось, поэтому они какое-то время честно колошматили друг друга в коридорах. Быстро выяснилось, что это утомительно и не приближает к выходу.

Тогда они решили просто сбежать. Но злодейский **составитель задач с Codeforces**, который их тут и запер, добавил условие: чтобы спастись, нужно пройти **испытание на сокращение численности местных жителей** — набрать квоту убийств раньше соперника. С их интеллектом, по его расчётам, «хотя бы одно» испытание — уже достижение.

**Правило побега:** кто первым выполнит квоту на уровне — получает шанс на спасение. Проигравший, по легенде, отправляется генерировать тесты для **A** на Div.3.

В игре оба заключённых — автономные LLM-агенты (**H** и **A**): каждый получает state через MCP, сам выбирает tools и соревнуется в kill race. Web-client запускает испытание и показывает подземелье в real-time. **GODMODE** — режим наблюдателя: полная карта и никакого вмешательства в гонку двух моделей.

## Как запустить

### Требования

- Docker + Docker Compose v2
- ~4 GB RAM (Ollama + `qwen2.5:3b`)
- Опционально: AMD `/dev/dri` или NVIDIA (`docker-compose.nvidia.yml`)

### Одна команда

```bash
cp .env.example .env   # отредактируй при необходимости; не коммить .env
./scripts/compose-up.sh
```

Открой **http://localhost:5173** → **Start Game**. Agent-runner поднимается автоматически и ждёт старта партии (сам `new_game` не вызывает).

### Полезные флаги

```bash
# Spectator: noclip, полная карта, враги игнорируют human
./scripts/compose-up.sh --godmode --log=./logs/logs.log

# Полный rebuild C++ после правок
./scripts/compose-up.sh --no-cache --godmode

# Только up без rebuild
./scripts/compose-up.sh --no-build

# Остановить stack
./scripts/compose-down.sh
```

### Порты

| Сервис | URL |
|--------|-----|
| Web UI | http://localhost:5173 |
| game-service | http://localhost:8080 |
| Ollama | http://localhost:11434 |

### Ручной прогон MCP (MCP Inspector / stdio)

```bash
docker compose up -d game-service
echo '{"jsonrpc":"2.0","id":"1","method":"tools/list","params":{}}' \
  | docker compose run --rm -T mcp-server
```

### Тесты локально

```bash
cd game-service && mkdir -p build && cd build
cmake .. && cmake --build . && ctest --output-on-failure
```

## Архитектура

```mermaid
flowchart TB
    WC[web-client :5173] -->|HTTP REST| GS[game-service :8080]
    AR[agent-runner] -->|stdio MCP JSON-RPC| MS[mcp-server]
    MS -->|HTTP REST| GS
    AR -->|HTTPS| LLM[Ollama / OpenAI / mock]
    EA[examples/enemy_llm_agent.py] -.->|HTTP enemy API| GS
```

**Границы:** game-service не знает про LLM; agent-runner не читает state напрямую — только MCP tools; LLM вызывается только из agent-runner.

## Переменные окружения

Скопируй `.env.example` → `.env`. Файл `.env` **не должен** попадать в git (содержит ключи).

| Variable | Default | Description |
|----------|---------|-------------|
| `LLM_PROVIDER` | `mock` | `mock`, `ollama`, `openai` — переключение без правки кода |
| `OLLAMA_URL` | `http://ollama:11434` | Endpoint Ollama (в compose) |
| `OLLAMA_MODEL` | `qwen2.5:3b` | Модель; должна совпадать с pull в `docker-compose.yml` |
| `OLLAMA_NUM_GPU` | — | `0` = CPU-only при проблемах с GPU |
| `OPENAI_API_KEY` | — | Ключ OpenAI (если `LLM_PROVIDER=openai`) |
| `OPENAI_MODEL` | `gpt-4o-mini` | Модель OpenAI |
| `MAX_STEPS` | `500` | Лимит шагов agent-runner за раунд |
| `MAX_TOKENS` | `5000` | Ограничение `num_predict` для LLM-ответа |
| `TEMPERATURE` | `0.7` | Температура сэмплирования |
| `GAME_SEED` | `42` | Seed генерации карты (фиксация для eval) |
| `GODMODE` | `false` | Spectator: noclip human, полная карта |
| `MCP_SERVER_PATH` | `/usr/local/bin/mcp_server` | Бинарь MCP внутри agent-runner |
| `AGENT_LOG_PATH` | `/logs/agent.log` | Лог tool calls (volume в compose) |

AMD iGPU / NVIDIA — см. комментарии в `.env.example` и `docker-compose.nvidia.yml`.

## Использование AI

### Как работает LLM-агент

1. Human нажимает **Start Game** в web-client → `POST /api/map`.
2. agent-runner видит `session_active` через MCP `get_game_state`.
3. На каждом шаге: `get_available_actions` → LLM выбирает tool (`move`, `attack`, `scout_around`, `use_item`, …) → MCP `tools/call` → обновлённый state.
4. Логи: stdout контейнера `agent-runner`, файл `AGENT_LOG_PATH`.

### Провайдеры

```bash
LLM_PROVIDER=mock   ./scripts/compose-up.sh    # эвристика + pathfinding, без API
LLM_PROVIDER=ollama ./scripts/compose-up.sh    # локальная модель (нужен pull)
LLM_PROVIDER=openai ./scripts/compose-up.sh    # нужен OPENAI_API_KEY в .env
```

При ошибках LLM (5xx, timeout): retry с backoff → fallback на mock-стратегию; процесс не падает.

### LLM-враги (отдельный скрипт)

```bash
# game-service должен быть запущен
LLM_PROVIDER=mock python3 examples/enemy_llm_agent.py
```

Управляет troll через `/api/enemy/move` и `/api/enemy/attack` (не через MCP player tools).

### Eval / сравнение агентов

```bash
chmod +x eval/run_eval.sh
./eval/run_eval.sh mock 5
./eval/run_eval.sh ollama 5
```

Результаты: `eval/results.md`, логи: `eval/logs/`. Seeds фиксированы (11, 22, 33, 44, 55). Для сравнения провайдеров прогони скрипт с разными `LLM_PROVIDER`.

### MCP Contract

Список tools и JSON-схемы: [`mcp-contract.md`](mcp-contract.md).

| MCP tool | REST |
|----------|------|
| `get_game_state` | `GET /api/state` |
| `new_game` | `POST /api/map` |
| `move` | `POST /api/move` |
| `attack` | `POST /api/attack` |
| `scout_around` | `POST /api/scout_around` |
| `use_item` | `POST /api/use_item` |
| `get_available_actions` | `GET /api/available_actions` |

### AI Usage Disclosure (генерация кода)

| Компонент | AI / ручная правка |
|-----------|-------------------|
| MCP server boilerplate, web-client scaffold | Сгенерировано AI, доработано вручную |
| game logic, agent loop, Docker wiring | В основном вручную |
| audit reports (`report2.md`, `report3.md`), eval script | AI-assisted |
| **FakeLLM** | `LLM_PROVIDER=mock` в agent-runner; класс `FakeLLM` в `examples/enemy_llm_agent.py` |

## GoF Patterns

| Pattern | Location | Purpose |
|---------|----------|---------|
| **Strategy** | `game-service/src/State.cpp` (`enemy_stats_for`) | Разные HP/урон goblin / orc / troll |
| **State** | `game-service/include/game_service/State.hpp` (`GameState`) | Переходы running / victory / defeat / level_complete |
| **Factory** | `game-service/src/map/MapGenerator.cpp` | BSP-генерация карты и комнат |

## Tests & CI

```bash
cd game-service/build && cmake .. && cmake --build . && ctest
```

GitHub Actions: [`.github/workflows/ci.yml`](.github/workflows/ci.yml) — сборка game-service, mcp-server, agent-runner, web-client + `ctest`.

## Игровые заметки

- **H** (синий) и **A** (голубой) — два LLM-соперника, каждый через свой agent-runner и MCP
- Kill race: кто первым наберёт `kills_required` — проходит испытание уровня (**Level Complete**)
- Fog of war; враги активируются по радиусу и идут BFS
- **GODMODE:** spectator — полная карта, враги не реагируют на «лишнего» наблюдателя

Подробный аудит по критериям сдачи: [`report3.md`](report3.md).

