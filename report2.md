# Аудит проекта по требованиям ДЗ2 (hw2_task.pdf)

**Репозиторий:** `game_with_llm_sd_project_2_blin_super_kruto_bimbim_bambam`  
**Ветка:** `feature/game_server_report`  
**Коммит:** `30ae4e8` (Code audit)  
**Дата аудита:** 14 июня 2026  
**Основание:** `hw2_task.pdf`, `mcp-contract.md`, `report.md`, runtime-проверки

---

## 1. Резюме

| Область | Оценка | Комментарий |
|---------|--------|-------------|
| Игровое ядро (game-service) | **7/10** | Рабочий roguelike: карта BSP, бой, инвентарь, победа/поражение |
| Графика (web-client) | **4/10** | Canvas-клиент есть, но **сломан** из-за расхождения JSON-схемы с бэкендом |
| MCP-сервер | **6/10** | 7 tools, проксирование на game-service работает; валидация аргументов слабая |
| Agent-runner + LLM | **1/10** | Обходит MCP, LLM не вызывается, нет agent loop |
| LLM-враги | **0/10** | Не реализованы |
| Сравнение агентов / eval | **0/10** | Нет `eval/`, скриптов, `results.md` |
| Качество (тесты, CI) | **0/10** | Нет юнит-тестов, нет GitHub Actions |
| GoF-паттерны | **0/10** | Не описаны в README |
| Docker Compose (one command) | **3/10** | 4 контейнера поднимаются, но пайплайн не связан, web-client вне compose |

**Общая готовность к финальной сдаче (10.06): ~25–30%**  
**Playable demo:** game-service по HTTP; web-client и AI-пайплайн требуют доработки.

---

## 2. Архитектура: требование vs реальность

### Требование (hw2_task.pdf)

```
agent-runner ──stdio MCP──▶ mcp-server ──HTTP──▶ game-service
       │
       └──HTTPS──▶ LLM provider
```

Границы: game-service не знает про LLM; mcp-server не знает про LLM; agent-runner не знает внутренний state — только MCP tools.  
Деплой: `docker compose up` без ручных шагов.

### Фактически

```mermaid
flowchart LR
    subgraph required["Требуется"]
        AR1[agent-runner] -->|stdio MCP| MS1[mcp-server]
        MS1 -->|HTTP| GS1[game-service]
        AR1 -->|HTTPS| LLM1[LLM]
    end

    subgraph actual["Реализовано"]
        WC[web-client] -->|HTTP напрямую| GS2[game-service]
        AR2[agent-runner] -->|HTTP напрямую| GS2
        MS2[mcp-server] -->|HTTP| GS2
        AR2 -.->|LLMClient создан, chat не вызывается| X[—]
        EA[examples/*.py] -->|HTTP enemy API| GS2
        OL[ollama] -.->|не используется| X
    end
```

| Нарушение границ | Статус |
|------------------|--------|
| agent-runner → game-service напрямую | **Да** — антипаттерн «читы агенту» |
| agent-runner → mcp-server stdio | **Нет** — контейнеры изолированы |
| LLM в agent loop | **Нет** |
| web-client в docker compose | **Нет** — ручной `npm run dev` |
| Ollama используется | **Нет** — контейнер качает модель (~сотни MB) впустую |

---

## 3. Матрица соответствия требованиям hw2_task.pdf

### 3.1. Игра

| Требование | Статус | Доказательство / замечание |
|------------|--------|---------------------------|
| Графика (не CLI) | ⚠️ | `web-client/` — Canvas + HUD; **управление заблокировано** (см. §5.3) |
| Герой: HP, инвентарь, **уровни** | ⚠️ | HP + инвентарь ✅; **уровней нет** |
| Случайная карта (комнаты + коридоры) | ✅ | BSP в `MapGenerator.cpp` |
| 3+ типа мобов с **разным поведением** | ❌ | Enum `Goblin/Orc/Troll`, но все: HP=10, урон=2, одинаковая логика |
| Боевая система | ✅ | Атака, сопротивление, смерть, дроп лута |
| Условие победы / проигрыша | ⚠️ | `PlayerVictory` / `EnemyVictory` в коде; **JSON-контракт не совпадает** с клиентом |

### 3.2. Архитектура (3+ микросервиса)

| Требование | Статус | Комментарий |
|------------|--------|-------------|
| game-service | ✅ | HTTP :8080, игровая логика |
| mcp-server | ⚠️ | JSON-RPC + 7 tools, HTTP-прокси (**исправлено** с report.md) |
| agent-runner | ❌ | Hardcoded цикл move, HTTP к game-service |
| docker compose up | ⚠️ | Сервисы стартуют, но **не интегрированы** в целевой пайплайн |
| Одна команда без ручных шагов | ❌ | web-client отдельно; agent не через MCP |

### 3.3. GoF-паттерны (минимум 3)

| Требование | Статус |
|------------|--------|
| 3+ паттерна с file:line в README | ❌ **Не описано** |

Потенциальные кандидаты (не задокументированы): BSP-генератор (`MapGenerator`), enum-serialization, но формального раздела в README нет.

### 3.4. AI-блок

| Требование | Статус | Комментарий |
|------------|--------|-------------|
| MCP 5+ tools | ✅ | 7 tools в `mcp-server/src/main.cpp` |
| MCP-контракт в репо | ✅ | `mcp-contract.md` |
| Ошибки при невалидных аргументах | ❌ | Segfault при `move` без `direction` (exit 139); invalid direction → `success:false`, не MCP error |
| Agent loop через MCP | ❌ | agent-runner не подключается к mcp-server |
| 2+ LLM-провайдера через env | ❌ | `LLMClient` — mock или `nullopt`; Ollama/OpenAI не реализованы |
| Budget (шаги/токены) | ⚠️ | `MAX_STEPS=10` в compose; токены не считаются |
| Graceful degradation LLM | ❌ | Нет retry/backoff/fallback |
| LLM-управляемые враги | ❌ | `examples/enemy_team_agent_example.py` — if/else chase, не LLM |
| 2+ агента, 5+ прогонов, метрики | ❌ | Нет eval-инфраструктуры |
| `eval/results.md` | ❌ | Каталог `eval/` отсутствует |

### 3.5. Качество

| Требование | Статус |
|------------|--------|
| Юнит-тесты (combat, pathfinding, инвентарь) | ❌ |
| Тесты agent loop с FakeLLM | ❌ |
| CI (push → тесты + линтер) | ❌ |
| README: one command, env, LLM switch, AI usage | ⚠️ Частично; нет раздела про AI-кодинг |

### 3.6. Антипаттерны (штрафные баллы)

| Антипаттерн | Обнаружен |
|-------------|-----------|
| Читы агенту (прямой доступ к state) | **Да** — `agent-runner` → HTTP `/api/state` |
| Хардкод одного LLM-провайдера | **Да** — реально работает только mock |
| Один прогон / один сид | **Да** — eval отсутствует |
| Нет budget | ⚠️ MAX_STEPS есть, но мал (10) |
| Тесты ходят в реальный LLM | N/A — тестов нет |
| API-ключи в Git | ⚠️ `.env.example` с placeholder; `.env` в `.gitignore` |

---

## 4. Аудит контрактов

### 4.1. mcp-contract.md ↔ game-service

**Критическое расхождение схемы состояния:**

| Поле (контракт) | Фактический JSON (`GET /api/state`) |
|-----------------|--------------------------------------|
| `game_over: bool` | **отсутствует** |
| `phase: "player_turn" \| ...` | **отсутствует** |
| `won: bool` | **отсутствует** |
| — | `game_state: "running" \| "player_victory" \| "enemy_victory"` |

Проверка (runtime, 14.06.2026):

```bash
curl -s http://localhost:8080/api/state | python3 -c \
  "import json,sys; d=json.load(sys.stdin); print(sorted(d.keys()))"
# ['enemies', 'game_state', 'map_loot', 'player', 'steps']
```

**Дополнительные расхождения:**

| Эндпоинт / поле | Контракт | Факт |
|-----------------|----------|------|
| `player.x/y` | ✅ | ✅ |
| `enemies[].type` | goblin | goblin/orc/troll ✅ |
| `enemies[].index` | не описан | присутствует в API |
| `player.current_damage_bonus/resistance` | не описаны | присутствуют |
| `Item` passive/active поля | не полностью | расширенная схема в коде |
| Turn-based `phase` | описан | заменён на real-time `GameState` (`REALTIME_ARCHITECTURE.md`) |

**Вывод:** `mcp-contract.md` описывает **устаревшую turn-based** модель; код перешёл на real-time, но контракт и клиенты не обновлены.

### 4.2. mcp-server ↔ game-service

| Tool | REST | Проксирование | Проверено |
|------|------|---------------|-----------|
| get_game_state | GET /api/state | ✅ | ✅ |
| move | POST /api/move | ✅ | ✅ |
| attack | POST /api/attack | ✅ | — |
| get_visible_cells | GET /api/visible_cells | ✅ | — |
| pickup_item | POST /api/pickup_item | ✅ | — |
| use_item | POST /api/use_item | ✅ | — |
| get_available_actions | GET /api/available_actions | ✅ | ✅ |

MCP `tools/call get_game_state` возвращает актуальный state с game-service (не stub — **исправление** относительно report.md от 09.06).

**Проблемы MCP:**
- `move` без обязательного `direction` → **SIGSEGV** (crash mcp-server)
- Невалидный `direction: "diagonal"` → HTTP-ответ `success:false`, но не JSON-RPC error с понятным сообщением (требование 4.1)
- Нет tool для `POST /api/map` (старт игры) — агент не может начать партию через MCP

### 4.3. web-client ↔ game-service

| Аспект | Статус |
|--------|--------|
| REST paths | ✅ Совпадают |
| `player.x/y` | ✅ |
| `phase` / `game_over` / `won` | ❌ **Клиент ожидает, бэкенд не отдаёт** |
| `MapOptions` | ✅ |
| `Corridor` тип `{x1,y1,x2,y2}` vs backend `Rect` | ⚠️ Коридоры не рисуются |

**Критический баг web-client** (`web-client/src/main.ts`):

```typescript
if (store.state?.phase !== 'player_turn' || store.state?.game_over) return;
```

API возвращает `game_state: "running"`, поле `phase` = `undefined`.  
`undefined !== 'player_turn'` → **true** → все действия (move, attack, pickup, use_item) **немедленно блокируются**.

Overlay победы/поражения проверяет `game_over` / `won` — поля отсутствуют → **UI никогда не покажет Victory/Game Over**, даже при `game_state: "player_victory"`.

### 4.4. agent-runner ↔ контракты

| Аспект | report.md (09.06) | Сейчас (14.06) |
|--------|-------------------|----------------|
| `player.pos` vs `player.x/y` | ❌ Падение | ✅ Исправлено |
| `game_over` / `won` | ❌ Не работает | ❌ Поля по-прежнему отсутствуют в API |
| MCP stdio | ❌ | ❌ |
| LLM `chat()` | ❌ Не вызывается | ❌ |
| Обход MCP (HTTP) | ❌ | ❌ Антипаттерн сохраняется |

---

## 5. Аудит компонентов

### 5.1. game-service — сильная сторона проекта

**Реализовано:**
- BSP-генерация карты с seed (`MapGenerator.cpp`)
- REST API: state, move, attack, pickup, use_item, visible_cells, available_actions, map, health
- Real-time API для врагов: `/api/enemies`, `/api/enemy/move`, `/api/enemy/attack`, `/api/enemy/visible_cells`
- Инвентарь: пассивные (меч +2 dmg, броня +20% resist) + активные (зелье +20 HP)
- Fog of war (круговая видимость)
- Победа: все враги убиты → `PlayerVictory`; смерть → `EnemyVictory`
- Сброс состояния в `start_new_game` (**исправлено**): clear enemies, loot, inventory, steps

**Баги / недочёты:**

| # | Проблема | Severity |
|---|----------|----------|
| 1 | JSON state не соответствует `mcp-contract.md` и web-client | **Critical** |
| 2 | 3 типа мобов — только label; HP/урон/AI одинаковые | **Major** (требование курса) |
| 3 | Уровни (levels) не реализованы | **Major** |
| 4 | `check_victory_conditions()` не вызывается в `move()` | Minor |
| 5 | Карта без врагов: victory только через `/api/update` или kill | Minor |
| 6 | `examples/player_agent_example.py` использует устаревшие MapOptions (`room_count`) | Minor |
| 7 | Real-time refactor частично: нет автоматического enemy AI в game-service | Info |

**Runtime-тесты (14.06.2026):**

```text
GET  /health                          → {"status":"ok"}
POST /api/map (seed=42)               → success, 12 enemies (goblin/orc/troll)
POST /api/move direction=right        → success: true
GET  /api/available_actions           → ["move"]
POST /api/map (0 enemies) + GET /api/update → game_state: player_victory ✅
docker compose run agent-runner       → 10 шагов, ходит по кругу, не падает ✅
```

### 5.2. mcp-server

**Плюсы:**
- JSON-RPC: `initialize`, `tools/list`, `tools/call`
- libcurl → game-service (не заглушка)
- 7 tools с inputSchema

**Минусы:**
- Crash на missing required args
- Нет валидации enum на уровне MCP до вызова handler
- Нет tool `start_game` / `new_game`
- stdio-режим не интегрирован в docker-compose с agent-runner

### 5.3. web-client

**Плюсы:**
- Vite + TypeScript, Canvas renderer, fog of war, HUD, WASD/click
- Типизированный REST-клиент, proxy на :8080

**Минусы (блокирующие):**
- Проверка `phase === 'player_turn'` делает игру **неуправляемой**
- Victory/Game Over overlay не работает
- Не входит в docker-compose

### 5.4. agent-runner

```cpp
// LLMClient создаётся, но chat() нигде не вызывается
auto llm = LLMClient::from_env();
// Стратегия: right → down → left → up
std::string direction = (step % 4 == 0) ? "right" : ...
```

- Нет MCP-клиента (stdio)
- Нет парсинга LLM-ответа → tool call
- Нет логирования tool calls
- Нет retry/backoff
- `LLM_PROVIDER=ollama` → `chat()` returns `nullopt`

### 5.5. examples/ (Python)

- `player_agent_example.py`, `enemy_team_agent_example.py` — HTTP-клиенты с простым if/else AI
- **Не LLM**, не MCP, не интегрированы в docker-compose
- Могут служить основой для enemy-team agent, но требование «LLM через tool use» не выполнено

### 5.6. ollama

- Контейнер в compose, pull `qwen2.5:0.5b` при старте
- Ни один серvice не вызывает `http://ollama:11434`

---

## 6. Изменения относительно report.md (09.06.2026)

| Проблема из report.md | Статус 14.06 |
|-----------------------|--------------|
| mcp-server — заглушки | ✅ **Исправлено** — HTTP proxy |
| agent-runner `player.pos` crash | ✅ **Исправлено** — `player.x/y` |
| `start_new_game` не очищает state | ✅ **Исправлено** |
| `steps` UB | ✅ **Исправлено** — `int steps{}` |
| Victory никогда не наступает | ✅ **Исправлено** — `check_victory_conditions` |
| Только goblin | ⚠️ **Частично** — 3 типа в enum, поведение одинаковое |
| MCP/agent/LLM не связаны | ❌ **Без изменений** |
| web-client phase mismatch | ❌ **Усугублено** real-time refactor |
| Юнит-тесты, CI, eval | ❌ **Без изменений** |

---

## 7. Чекпойнты (hw2_task.pdf)

| Дата | Требование | Оценка выполнения |
|------|------------|-------------------|
| 20.05 | diagram + MCP contract + 3 stub services | ✅ Выполнено |
| 31.05 | игра руками; MCP state; агент дошёл до конца | ⚠️ game-service играется по HTTP; MCP state OK; agent ходит, но не «до конца» и не через MCP |
| 10.06 | eval-скрипт, 2+ агента, логи, отчёт | ❌ Не выполнено |
| Защита (~середина июня) | live demo full stack | ❌ Не готово |

---

## 8. Оценка работоспособности системы

### Что работает end-to-end

```bash
# 1. Бэкенд
docker compose up --build game-service
curl http://localhost:8080/health

# 2. Новая игра + ход
curl -X POST http://localhost:8080/api/map -H 'Content-Type: application/json' \
  -d '{"map_width":40,"map_height":30,"min_node_size":8,"max_depth":4,"seed":42}'
curl -X POST http://localhost:8080/api/move -H 'Content-Type: application/json' \
  -d '{"direction":"right"}'

# 3. MCP (отдельный контейнер)
echo '{"jsonrpc":"2.0","id":"1","method":"tools/call","params":{"name":"get_game_state","arguments":{}}}' \
  | docker compose run --rm -T mcp-server

# 4. Agent-runner (HTTP, не MCP)
docker compose run --rm --no-deps agent-runner
```

### Что не работает / неполно

| Сценарий | Результат |
|----------|-----------|
| `docker compose up` → игра в браузере | ❌ web-client не в compose; клиент сломан по phase |
| agent-runner → MCP → game | ❌ |
| LLM принимает решения | ❌ |
| LLM-враги | ❌ |
| Сравнение 2 агентов | ❌ |
| Victory UI в web-client | ❌ |
| agent-runner детектит победу | ❌ (`won`/`game_over` отсутствуют) |

---

## 9. Приоритетный план (до защиты)

### P0 — блокеры защиты

1. **Унифицировать JSON state** — либо вернуть `phase`/`game_over`/`won` в `to_json`, либо обновить `mcp-contract.md`, web-client, agent-runner на `game_state`
2. **agent-runner → stdio MCP** — убрать прямой HTTP (устранить антипаттерн)
3. **Agent loop:** state → LLM → tool call → repeat; реализовать Ollama + второй провайдер
4. **MCP:** tool `new_game`, валидация args без crash
5. **docker-compose:** связать agent-runner | mcp-server через pipe/supervisor; добавить web-client

### P1 — требования курса

6. LLM enemy agent (минимум 1 тип моба) через tool use
7. Разное поведение/статы goblin/orc/troll
8. GoF: описать 3 паттерна в README с file:line
9. `eval/run_eval.sh` + 2 агента × 5+ seeds → `eval/results.md`
10. Юнит-тесты + GitHub Actions CI

### P2 — качество

11. README: фактическое состояние, AI usage disclosure
12. FakeLLM тесты agent loop
13. Уровни (levels) или обоснование упрощения

---

## 10. Итоговая таблица баллов (ориентировочно)

| Блок | Max (условно) | Факт | % |
|------|---------------|------|---|
| Игра | 20 | 12 | 60% |
| Архитектура | 15 | 5 | 33% |
| GoF | 10 | 0 | 0% |
| AI / MCP | 25 | 4 | 16% |
| Eval / agents | 15 | 0 | 0% |
| Качество | 15 | 2 | 13% |
| **Итого** | **100** | **~23** | **~23%** |

*Оценка ориентировочная; реальная оценка преподавателя может отличаться.*

---

## 11. Вывод

**Сильные стороны:** `game-service` — работоспособное ядро roguelike с генерацией карты, боем, инвентарём, real-time API для двух агентов. `mcp-server` после доработки проксирует tools на бэкенд. Часть багов из первого аудита (`report.md`) исправлена.

**Главный разрыв:** проект описывает и частично реализует **две несовместимые архитектуры** — turn-based (контракт, web-client) и real-time (код game-service). AI-блок (MCP agent loop, LLM, eval) **не доведён до рабочего состояния**. Для защиты критично: синхронизировать контракты, починить web-client, связать agent-runner с MCP и LLM, добавить eval.

**Минимальный demo для защиты (если мало времени):**
1. Fix JSON schema + web-client input
2. agent-runner через MCP + mock LLM agent loop
3. Один eval-прогон с логами

Полноценное соответствие hw2_task.pdf потребует существенной доработки AI-блока и инфраструктуры качества.
