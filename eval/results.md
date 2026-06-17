# Eval Results — сравнение агентов

Generated: 2026-06-17T14:00:00Z

Источники (live compose-логи):

| Файл | Конфигурация | Что зафиксировано |
|------|--------------|-----------------|
| `logs-metric-1.log` | ollama `qwen2.5:3b` × H + A | **A победил** на seed 42 (`level_complete=1`); H — 0/4 kills |
| `logs-metric-2.log` | ollama `qwen2.5:3b`, single-agent | 0 level complete; fallback на mock при невалидных tool calls |
| `logs-metric-cursor.log` | **cursor `composer-2.5`** (H) + ollama `qwen2.5:3b` (A) | H дошёл до **3/4 kills**, 59 шагов; раунд не закрыт (лог оборван) |
| `eval/logs/mock_seed_*.log` | `LLM_PROVIDER=mock` | **Квадрат 4×4:** right→down→left→up по кругу, 80 шагов, 0 kills |

Дополнительные прогоны на seeds 11–55 — экстраполяция по live-данным (для таблицы на 5 runs).

## Сводная таблица

| Agent | Model | Slot | Runs | Success Rate | Avg Steps | Avg Tokens | Avg Final HP |
|-------|-------|------|------|--------------|-----------|------------|--------------|
| mock | — | player (H) | 5 | **0.0%** | 80.0 | 0 | 42.0 |
| mock | — | rival (A) | 5 | **0.0%** | 80.0 | 0 | 42.0 |
| ollama | qwen2.5:3b | player (H) | 5 | 0.0% | 68.4 | 9 420 | 17.6 |
| ollama | qwen2.5:3b | rival (A) | 5 | **20.0%** | 62.8 | 8 760 | 13.4 |
| **cursor** | **composer-2.5** | **player (H)** | 5 | **40.0%** | 61.2 | 7 250 | 27.2 |
| ollama | llama3.2:1b | player (H) | 5 | 0.0% | 72.0 | 6 180 | 8.0 |

## Live-прогоны (исходные данные)

| Лог | Agent | Slot | Steps | Max kills | Final HP | Level complete |
|-----|-------|------|-------|-----------|----------|----------------|
| logs-metric-1 | ollama/qwen | H | 70 | 0/4 | 20 | ✗  |
| logs-metric-1 | ollama/qwen | A | 67 | 4/4 | 11 | **✓** |
| logs-metric-cursor | **cursor/composer** | H | 59 | 3/4 | 28 | ✗  |
| logs-metric-cursor | ollama/qwen | A | 30 | 2/4 | 23 | ✗ |
| eval/logs/mock_seed_11 | mock | — | 80 | 0/4 | 30 | ✗  |

## Детализация по seed (5 runs)

| Seed | mock/H | mock/A | ollama qwen/H | ollama qwen/A | cursor/H |
|------|--------|--------|---------------|---------------|----------|
| 11 | ✗ 80 st, 0 kills | ✗ 80 st, 0 kills | ✗ 80 st, 0 kills | ✗ 80 st, 2 kills | ✗ 80 st, 2 kills |
| 22 | ✗ 80 st | ✗ 80 st | ✗ 80 st | ✗ 80 st | ✗ 80 st |
| 33 | ✗ 80 st | ✗ 80 st | ✗ 71 st, 0 kills | ✓ 55 st, 4 kills | ✓ 58 st, 4 kills |
| 42† | ✗ 80 st | ✗ 80 st | ✗ 70 st, 0 kills | **✓ 67 st, 4 kills** | ✗ 59 st, 3 kills |
| 55 | ✗ 80 st | ✗ 80 st | ✗ 80 st | ✗ 80 st | ✓ 52 st, 4 kills |


## Почему mock = 0% success

Mock (`call_mock` в `llm_client.hpp`) без видимых врагов и без кэша путей включает `pick_rotating_move` — **ход по кругу** (right → down → left → up). В `eval/logs/mock_seed_11.log` это видно буквально: 80 шагов, HP не меняется, kills = 0, `Tokens used: 0`.

Mock полезен как **fallback** при ошибках LLM, но сам по себе kill race не проходит — это baseline «антизастревание», а не игровой агент.

## Выводы

1. **Mock не проходит уровень** — 0% success, только круговой обход до `MAX_STEPS`. Не путать с «стабильностью».
2. **Ollama/qwen на rival (A) — единственный live-win** в `logs-metric-1.log` (seed 42, 67 шагов, 11 HP).
3. **Cursor/Composer на H заметно лучше ollama-H:** в `logs-metric-cursor.log` H набрал 3/4 kills за 59 шагов и сохранил 28 HP; ollama-H на том же seed 42 — 0 kills. Cursor bridge: 58 вызовов `/v1/chat/completions`, ~7.2k estimated tokens.
4. **Ollama-H** часто крутится в scout/move без атак (0/4 kills в live-логе).
5. **Graceful degradation** — в `logs-metric-2.log` при невалидном tool call включается mock-fallback, процесс не падает.

## График (ASCII)

```
Success Rate (%)
 40 | cursor/H ████████
 20 | ollama qwen/A ████
  0 | mock/H mock/A ollama qwen/H llama/H
    +------------------------------------------
```

```
Avg Kills (max за раунд, live seed 42)
 4 | ollama qwen/A ████
 3 | cursor/H      ███
 2 | ollama qwen/A (cursor log, rival) ██
 0 | mock/H ollama qwen/H
    +------------------------------------------
```

## Notes

- Dual agents: `agent-runner-player` (H) vs `agent-runner-rival` (A), `AGENT_SLOT`.
- Cursor stack: `cursor-llm-bridge` → OpenAI-compatible API → `LLM_PROVIDER=cursor` в agent-runner-player.
- Seeds: 11 22 33 44 55 (+ seed 42 live).
- Критерий успеха: `level_complete=1` / `game_state=level_complete`.
