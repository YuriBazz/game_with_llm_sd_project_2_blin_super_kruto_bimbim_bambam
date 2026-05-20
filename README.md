# game_with_llm_sd_project_2_blin_super_kruto_bimbim_bambam

Roguelike игра с LLM-агентом, реализованная на C++ в микросервисной архитектуре с MCP протоколом.

## Требования курса

- **Архитектура**: 3+ микросервиса (game-service, mcp-server, agent-runner, ollama)
- **Игровая часть**: Графика, случайная генерация карты, 3+ типа мобов, боевая система, инвентарь
- **AI блок**: MCP сервер с 5+ tools, agent loop с LLM, LLM-управляемые враги
- **Качество**: Юнит-тесты, CI, README, защита

## Архитектура

```mermaid
flowchart TB
    subgraph External["Внешние системы"]
        Ollama[Ollama LLM<br/>qwen2.5:0.5b/7b]
    end

    subgraph GameCore["Игровое ядро (C++ 17)"]
        GS[game-service<br/>HTTP сервер<br/>:8080]
        MS[mcp-server<br/>MCP протокол<br/>stdio]
        AR[agent-runner<br/>Agent loop<br/>HTTP клиент]
    end

    subgraph Storage["Хранилища"]
        Vol[ollama_data<br/>Volume Docker]
    end

    %% Связи
    AR -->|HTTP /v1/chat| Ollama
    AR -->|stdio JSON-RPC| MS
    MS -->|HTTP REST| GS
    Ollama -.->|сохраняет модели| Vol

    %% Стилизация
    style GS fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px
    style MS fill:#fff3e0,stroke:#ed6c02,stroke-width:2px
    style AR fill:#e3f2fd,stroke:#1565c0,stroke-width:2px
    style Ollama fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px
    style Vol fill:#f5f5f5,stroke:#616161,stroke-width:1px,stroke-dasharray: 5 5
```
## Технологии

  - C++17
  - CMake
  - Docker + Docker Compose
  - Ollama (qwen2.5)
  - nlohmann/json
  - libcurl
  - MCP Protocol

## Быстрый старт
  ### 1. Запуск проекта
  ```bash
  docker compose up --build
  ```

  ### 2. Переменные окружения
  ```bash
  cp .env.example .env
  ```
  ### Основные настройки:
  - LLM_PROVIDER=mock (по умолчанию) или ollama
  - MAX_STEPS=50

## MCP-контракт

  Подробно расписано в mcp-contract.md
