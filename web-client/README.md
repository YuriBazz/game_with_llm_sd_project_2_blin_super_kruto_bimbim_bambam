# Roguelike Browser Client

Browser-based UI for roguelike game backend (game-service). Displays procedurally generated dungeon, handles player movement, combat, inventory, and fog of war.

## Features

- **Real-time rendering**: Canvas-based map with fog of war
- **Responsive controls**: Keyboard (WASD/Arrows) and mouse-based interaction
- **HUD display**: HP, gold, inventory, turn phase, step counter
- **No client-side logic**: All game rules enforced on backend
- **Retro theme**: Dark colors, monospace font

## Installation

```bash
npm install
```

## Running

### Development mode with Vite proxy:

```bash
npm run dev
```

Then open http://localhost:5173 in your browser.

### Ensure backend is running:

```bash
# Backend should be on http://localhost:8080
docker-compose up game-service
```

### Production build:

```bash
npm run build
# Serve dist/ folder
```

## Architecture

```
src/
├── main.ts              # Application entry point & event handling
├── api/
│   └── gameClient.ts    # Typed REST API wrapper
├── types/
│   └── game.ts          # TypeScript interfaces for game objects
├── render/
│   └── mapRenderer.ts   # Canvas rendering with fog of war
├── ui/
│   └── hud.ts           # HUD display & game log
└── state/
    └── gameStore.ts     # Local game state cache
```

## Controls

| Input | Action |
|-------|--------|
| **W** / **↑** | Move up |
| **S** / **↓** | Move down |
| **A** / **←** | Move left |
| **D** / **→** | Move right |
| **Space** / **G** | Pickup item |
| **Click enemy** | Attack adjacent enemy |
| **Click inventory item** | Use item |

## API Communication

All requests go through Vite proxy to `http://localhost:8080`:

- Health check: `GET /health`
- New game: `POST /api/map`
- Game state: `GET /api/state`
- Movement: `POST /api/move`
- Attack: `POST /api/attack`
- Pickup: `POST /api/pickup_item`
- Use item: `POST /api/use_item`
- Visible cells: `GET /api/visible_cells?radius=8`
- Available actions: `GET /api/available_actions`

## Game Rules (Backend)

- **Player HP**: 30
- **Player damage**: 5 per attack
- **Potion heal**: 20 HP
- **Fog of war**: Circular visibility (default radius 8)
- **Turn phases**: player_turn → enemy_turn → repeat or game_over/victory

## Development Notes

- Strict TypeScript enabled
- No external UI frameworks
- Canvas rendering with circle-based fog of war
- Responsive camera following player
- Game state synchronized with server after every action

## Troubleshooting

**"Failed to connect to backend"**: Ensure `game-service` is running on port 8080.

**CORS errors**: Vite proxy is configured in `vite.config.ts` to redirect `/api` to localhost:8080.

**Fog of war not updating**: Server returns visible cells in `GET /api/visible_cells`. Check backend is computing correctly.
