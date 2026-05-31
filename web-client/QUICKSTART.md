# Quick Start Guide - Roguelike Web Client

## Prerequisites

- **Node.js 18+** (with npm)
- **game-service backend** running on port 8080
- **Modern browser** (Chrome, Firefox, Safari, Edge)

## Installation & Running

### 1. Install Dependencies

```bash
cd web-client
npm install
```

### 2. Start Backend (in separate terminal)

```bash
docker-compose up game-service
```

Wait for: `[game_service] Game service listening on port 8080`

### 3. Start Development Server

```bash
cd web-client
npm run dev
```

Output will show:
```
  VITE v5.x.x  ready in 123 ms

  ➜  Local:   http://localhost:5173/
```

### 4. Open in Browser

Navigate to **http://localhost:5173**

You should see:
- Header with "New Game" button
- Empty canvas (left side)
- HUD panel and controls info (right side)

### 5. Start Playing

1. Click **[New Game]** button
2. Game generates map, player appears
3. Use controls to move and explore

## Troubleshooting

### "Cannot connect to backend"
- Verify `game-service` is running: `curl http://localhost:8080/health`
- Should respond: `{"status":"ok","service":"game-service"}`

### "Blank screen after clicking New Game"
- Check browser console: F12 → Console tab
- Should show game log messages
- If error about `/api/map`, ensure proxy is working

### CORS errors in console
- This is expected with direct backend access
- Vite proxy should intercept `/api` requests
- Check `vite.config.ts` has correct proxy config

### Port 5173 already in use
```bash
npm run dev -- --port 5174
```

## Controls Reference

| Key | Action |
|-----|--------|
| **W** or **Up Arrow** | Move up |
| **S** or **Down Arrow** | Move down |
| **A** or **Left Arrow** | Move left |
| **D** or **Right Arrow** | Move right |
| **Space** or **G** | Pick up item on current tile |
| **Click adjacent enemy** | Attack (must be next to enemy) |
| **Click inventory item** | Use item (e.g., potion) |
| **[New Game]** button | Start new game |

## UI Explanation

### Canvas (Left)
- Blue square (@) = Player
- Red squares (G/O/T) = Enemies (Goblin/Orc/Troll)
- Yellow dots ($) = Loot on ground
- Dark gray = Walls, Light gray = Floor
- Dark overlay = Fog of war (not visible)

### HUD Panel (Right Top)
- **Player**: Current HP/max HP, Gold amount
- **Steps**: Turn counter
- **Phase**: Shows "player_turn" in green or "enemy_turn" in red
- **Inventory**: Items you carry, click [Use] to consume

### Info Panel (Right Middle)
- Control reference
- Fog of war explanation

### Log Panel (Right Bottom)
- Last 10 messages
- Shows your moves, attacks, damage, pickups, heals
- Timestamps for each event

## Game Rules

- **Starting HP**: 30
- **Damage per attack**: 5 (always)
- **Potion heal**: 20 HP
- **Turn order**: Player acts → All enemies act → Next turn
- **Win condition**: Reach end of dungeon or defeat all enemies
- **Lose condition**: HP drops to 0

## Advanced: Production Build

```bash
npm run build
# Creates optimized dist/ folder
# Serve with: npx http-server dist
```

## Development Notes

- **TypeScript**: Strict mode enabled
- **API client**: `src/api/gameClient.ts` (typed fetch)
- **Render**: Canvas at 20px cells with camera system
- **State**: Synchronized with server after every action
- **No game logic on client**: All rules enforced by backend

---

**Issues or questions?** Check:
- README.md in web-client/
- mcp-contract.md for API specs
- game-service logs: `docker logs <container-id>`
