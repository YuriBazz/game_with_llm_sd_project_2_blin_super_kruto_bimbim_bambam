import { GameClient } from './api/gameClient';
import { GameStore } from './state/gameStore';
import { MapRenderer } from './render/mapRenderer';
import { GameLog, HUD } from './ui/hud';
import { isPlayable, isVictory, isDefeat, isLevelComplete, type GameState } from './types/game';

let client: GameClient;
let store: GameStore;
let renderer: MapRenderer;
let gameLog: GameLog;
let hud: HUD;

let isInputLocked = false;
let gameInitialized = false;
let pollTimer: ReturnType<typeof setInterval> | null = null;

function hideStartScreen(): void {
  const el = document.getElementById('startOverlay');
  if (el) el.style.display = 'none';
}

async function syncVisibleCells(state: GameState): Promise<void> {
  if (state.god_mode) return;
  const visibleCells = await client.getVisibleCells(8);
  store.updateVisibleCells(visibleCells);
}

function showStartScreen(): void {
  const el = document.getElementById('startOverlay');
  if (el) el.style.display = 'flex';
}

async function refreshState(): Promise<void> {
  if (!gameInitialized) return;
  const state = await client.tick();
  store.updateState(state);
  render();
  updateGodModeBanner();
}

function startPolling(): void {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = setInterval(async () => {
    if (!gameInitialized || !store.state) return;
    if (isVictory(store.state) || isDefeat(store.state) || isLevelComplete(store.state)) return;
    try {
      await refreshState();
    } catch {
      // ignore transient network errors during poll
    }
  }, 400);
}

async function startGame(mode: 'start' | 'reset' | 'next_level' = 'start'): Promise<void> {
  try {
    isInputLocked = true;
    hideStartScreen();
    hideLevelCompleteOverlay();

    await client.health();
    const state = await client.newGame({ mode });
    store.updateState(state);

    const map = await client.getMap();
    store.updateMap(map);

    if (!state.god_mode) {
      const visibleCells = await client.getVisibleCells(8);
      store.updateVisibleCells(visibleCells);
    }

    gameInitialized = true;
    isInputLocked = false;
    gameLog.clear();
    if (state.god_mode) {
      gameLog.add('GOD MODE — WASD moves you (H, noclip). Agent plays as A.');
      renderer.setFreeCamera(false, state.player.x, state.player.y);
    } else if (mode === 'next_level') {
      gameLog.add(`Level ${state.campaign_level ?? 1} started! Kill ${state.kills_required ?? 4} monsters.`);
      renderer.setFreeCamera(false);
    } else {
      gameLog.add('Game started! You are H — agent is A. Race to kill quota.');
      renderer.setFreeCamera(false);
    }
    startPolling();
    render();
    updateGodModeBanner();
  } catch (error) {
    console.error('Failed to initialize game:', error);
    gameLog.add(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    if (!gameInitialized) showStartScreen();
    isInputLocked = false;
  }
}

async function initializeGame(): Promise<void> {
  await startGame('start');
}

async function movePlayer(direction: 'up' | 'down' | 'left' | 'right'): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (!store.state || !isPlayable(store.state)) return;

  try {
    isInputLocked = true;
    const response = await client.move(direction);

    if (response.state) {
      store.updateState(response.state);
      await syncVisibleCells(response.state);

      if (response.success) {
        const label = store.state.god_mode ? 'H' : 'You';
        gameLog.add(`${label} moved ${direction} to (${response.x}, ${response.y})`);
      }
    }

    render();
  } catch (error) {
    console.error('Move failed:', error);
    gameLog.add(`Move failed: ${error instanceof Error ? error.message : 'Error'}`);
  } finally {
    isInputLocked = false;
  }
}

function hideLevelCompleteOverlay(): void {
  const el = document.getElementById('levelCompleteOverlay');
  if (el) el.style.display = 'none';
}

function showLevelCompleteOverlay(): void {
  const el = document.getElementById('levelCompleteOverlay');
  if (el) el.style.display = 'flex';
}

async function attackTarget(x: number, y: number): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (!store.state || !isPlayable(store.state)) return;

  const { player } = store.state;
  const dist = Math.abs(x - player.x) + Math.abs(y - player.y);
  if (dist !== 1) {
    gameLog.add('Target must be adjacent to attack');
    return;
  }

  try {
    isInputLocked = true;
    const response = await client.attack(x, y);

    if (response.state) {
      store.updateState(response.state);
      await syncVisibleCells(response.state);

      if (response.success) {
        gameLog.add(
          `Attacked (${x}, ${y}) for ${response.damage} dmg${response.target_dead ? ' — KILL!' : ''}`
        );
      } else {
        gameLog.add('Attack failed — nothing to hit there');
      }
    }

    render();
  } catch (error) {
    console.error('Attack failed:', error);
    gameLog.add(`Attack failed: ${error instanceof Error ? error.message : 'Error'}`);
  } finally {
    isInputLocked = false;
  }
}

async function attackNearestTarget(): Promise<void> {
  if (!store.state) return;
  const { player, enemies } = store.state;
  for (const enemy of enemies) {
    if (Math.abs(enemy.x - player.x) + Math.abs(enemy.y - player.y) === 1) {
      await attackTarget(enemy.x, enemy.y);
      return;
    }
  }
  gameLog.add('No adjacent target to attack');
}

async function useItem(itemId: string): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (!store.state || !isPlayable(store.state)) return;

  try {
    isInputLocked = true;
    const response = await client.useItem(itemId);

    if (response.state) {
      store.updateState(response.state);

      if (response.success) {
        gameLog.add(`Used item: ${response.effect} +${response.value}`);
      } else {
        gameLog.add('Cannot use item');
      }
    }

    render();
  } catch (error) {
    console.error('Use item failed:', error);
    gameLog.add(`Item use failed: ${error instanceof Error ? error.message : 'Error'}`);
  } finally {
    isInputLocked = false;
  }
}

function updateGodModeBanner(): void {
  const banner = document.getElementById('godModeBanner');
  if (!banner) return;
  banner.style.display = store.state?.god_mode ? 'inline-block' : 'none';
}

function updateLevelCompleteOverlay(state: GameState): void {
  const msg = document.getElementById('levelCompleteMessage');
  if (!msg) return;

  if (state.level_winner === 'human') {
    msg.textContent = 'Level complete! Human (H) reached kill quota first.';
  } else if (state.level_winner === 'ai') {
    msg.textContent = 'Level complete! Agent (A) reached kill quota first.';
  } else {
    msg.textContent = 'Level complete!';
  }
}

function render(): void {
  if (!store.state || !store.map) return;

  const { player, rival } = store.state;
  renderer.updateCamera(player.x, player.y);
  renderer.render(store);

  const phaseLabel = store.state.phase === 'realtime' ? 'Realtime' : store.state.phase;
  hud.render(
    player.hp,
    player.max_hp,
    player.gold,
    phaseLabel,
    store.state.steps,
    player.inventory,
    {
      level: store.state.campaign_level ?? 1,
      playerKills: store.state.player_kills ?? 0,
      rivalKills: store.state.rival_kills ?? 0,
      killsRequired: store.state.kills_required ?? 4,
    },
    {
      respawns: player.respawns_remaining ?? 0,
    },
    rival ? {
      hp: rival.hp,
      maxHp: rival.max_hp,
      respawns: rival.respawns_remaining ?? 0,
    } : undefined
  );

  const overlay = document.getElementById('overlay');
  if (overlay) {
    overlay.style.display = 'none';

    if (isLevelComplete(store.state)) {
      updateLevelCompleteOverlay(store.state);
      showLevelCompleteOverlay();
    } else if (isVictory(store.state) || isDefeat(store.state)) {
      overlay.style.display = 'flex';
      const message = overlay.querySelector('.message');
      if (message) {
        if (isVictory(store.state)) {
          message.textContent = `VICTORY! Steps: ${store.state.steps}`;
          overlay.style.backgroundColor = 'rgba(0, 200, 0, 0.8)';
        } else if (isDefeat(store.state)) {
          message.textContent = `GAME OVER! Steps: ${store.state.steps}`;
          overlay.style.backgroundColor = 'rgba(200, 0, 0, 0.8)';
        }
      }
    } else {
      hideLevelCompleteOverlay();
    }
  }

  gameLog.render(document.getElementById('log') || document.body);
  updateGodModeBanner();
}

function setupEventListeners(): void {
  document.addEventListener('keydown', (e: KeyboardEvent) => {
    if (e.repeat) return;

    switch (e.code) {
      case 'KeyW':
      case 'ArrowUp':
        e.preventDefault();
        movePlayer('up');
        break;
      case 'KeyS':
      case 'ArrowDown':
        e.preventDefault();
        movePlayer('down');
        break;
      case 'KeyA':
      case 'ArrowLeft':
        e.preventDefault();
        movePlayer('left');
        break;
      case 'KeyD':
      case 'ArrowRight':
        e.preventDefault();
        movePlayer('right');
        break;
      case 'Space':
        if (store.state?.god_mode) break;
        e.preventDefault();
        attackNearestTarget();
        break;
      default:
        break;
    }
  });

  const canvas = document.getElementById('gameCanvas') as HTMLCanvasElement;
  if (canvas) {
    canvas.addEventListener('click', (e: MouseEvent) => {
      if (!gameInitialized || !store.state) return;

      const rect = canvas.getBoundingClientRect();
      const screenX = e.clientX - rect.left;
      const screenY = e.clientY - rect.top;

      const { x: worldX, y: worldY } = renderer.getWorldCoordinates(screenX, screenY);
      const { player } = store.state;

      const dx = Math.abs(worldX - player.x);
      const dy = Math.abs(worldY - player.y);

      if (dx + dy !== 1) return;

      const enemy = store.getEnemyAt(worldX, worldY);
      if (enemy && !store.state.god_mode) {
        attackTarget(worldX, worldY);
        return;
      }

      if (store.state.god_mode || !store.isWall(worldX, worldY)) {
        if (worldX > player.x) movePlayer('right');
        else if (worldX < player.x) movePlayer('left');
        else if (worldY > player.y) movePlayer('down');
        else if (worldY < player.y) movePlayer('up');
      }
    });
  }

  const hudContainer = document.getElementById('hud');
  if (hudContainer) {
    hudContainer.addEventListener('click', (e: MouseEvent) => {
      const target = e.target as HTMLElement;
      if (target.dataset.itemId) {
        useItem(target.dataset.itemId);
      }
    });
  }

  const startBtn = document.getElementById('startGameBtn');
  if (startBtn) {
    startBtn.addEventListener('click', initializeGame);
  }

  const newGameBtn = document.getElementById('newGameBtn');
  if (newGameBtn) {
    newGameBtn.addEventListener('click', () => startGame('reset'));
  }

  const levelNewGameBtn = document.getElementById('levelNewGameBtn');
  if (levelNewGameBtn) {
    levelNewGameBtn.addEventListener('click', () => startGame('reset'));
  }

  const nextLevelBtn = document.getElementById('nextLevelBtn');
  if (nextLevelBtn) {
    nextLevelBtn.addEventListener('click', () => startGame('next_level'));
  }
}

function main(): void {
  client = new GameClient('');
  store = new GameStore();
  renderer = new MapRenderer(document.getElementById('gameCanvas') as HTMLCanvasElement);
  gameLog = new GameLog();
  hud = new HUD(document.getElementById('hud') as HTMLElement);

  renderer.setSize(800, 600);
  setupEventListeners();
  showStartScreen();
}

main();
