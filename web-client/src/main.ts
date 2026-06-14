import { GameClient } from './api/gameClient';
import { GameStore } from './state/gameStore';
import { MapRenderer } from './render/mapRenderer';
import { GameLog, HUD } from './ui/hud';

let client: GameClient;
let store: GameStore;
let renderer: MapRenderer;
let gameLog: GameLog;
let hud: HUD;

let isInputLocked = false;
let gameInitialized = false;

async function initializeGame(): Promise<void> {
  try {
    isInputLocked = true;

    // Verify backend is running
    await client.health();

    // Create new game
    const state = await client.newGame();
    store.updateState(state);

    // Load map
    const map = await client.getMap();
    store.updateMap(map);

    // Load visible cells
    const visibleCells = await client.getVisibleCells(8);
    store.updateVisibleCells(visibleCells);

    gameInitialized = true;
    isInputLocked = false;
    gameLog.clear();
    gameLog.add('Game started!');
    render();
  } catch (error) {
    console.error('Failed to initialize game:', error);
    gameLog.add(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    isInputLocked = false;
  }
}

async function movePlayer(direction: 'up' | 'down' | 'left' | 'right'): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (store.state?.phase !== 'player_turn' || store.state?.game_over) return;

  try {
    isInputLocked = true;
    const response = await client.move(direction);

    if (response.state) {
      store.updateState(response.state);
      const visibleCells = await client.getVisibleCells(8);
      store.updateVisibleCells(visibleCells);

      if (response.success) {
        gameLog.add(`Moved ${direction} to (${response.x}, ${response.y})`);
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

async function attackEnemy(x: number, y: number): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (store.state?.phase !== 'player_turn' || store.state?.game_over) return;

  try {
    isInputLocked = true;
    const response = await client.attack(x, y);

    if (response.state) {
      store.updateState(response.state);
      const visibleCells = await client.getVisibleCells(8);
      store.updateVisibleCells(visibleCells);

      if (response.success) {
        gameLog.add(
          `Attacked enemy at (${x}, ${y}) for ${response.damage} damage${response.target_dead ? ' - DEFEATED!' : ''}`
        );
      } else {
        gameLog.add('Attack failed!');
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

async function pickupItem(): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (store.state?.phase !== 'player_turn' || store.state?.game_over) return;

  try {
    isInputLocked = true;
    const response = await client.pickupItem();

    if (response.state) {
      store.updateState(response.state);

      if (response.success && response.items) {
        gameLog.add(`Picked up ${response.items.map(i => i.name).join(', ')}`);
      } else {
        gameLog.add('Nothing to pick up here');
      }
    }

    render();
  } catch (error) {
    console.error('Pickup failed:', error);
    gameLog.add(`Pickup failed: ${error instanceof Error ? error.message : 'Error'}`);
  } finally {
    isInputLocked = false;
  }
}

async function useItem(itemId: string): Promise<void> {
  if (isInputLocked || !gameInitialized) return;
  if (store.state?.phase !== 'player_turn' || store.state?.game_over) return;

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

function render(): void {
  if (!store.state || !store.map) return;

  const { player } = store.state;
  renderer.updateCamera(player.x, player.y);
  renderer.render(store);

  hud.render(
    player.hp,
    player.max_hp,
    player.gold,
    store.state.phase,
    store.state.steps,
    player.inventory
  );

  // Show overlays
  const overlay = document.getElementById('overlay');
  if (overlay) {
    overlay.style.display = 'none';

    if (store.state.game_over || store.state.won) {
      overlay.style.display = 'flex';
      const message = overlay.querySelector('.message');
      if (message) {
        if (store.state.won) {
          message.textContent = `VICTORY! Steps: ${store.state.steps}`;
          overlay.style.backgroundColor = 'rgba(0, 200, 0, 0.8)';
        } else {
          message.textContent = `GAME OVER! Steps: ${store.state.steps}`;
          overlay.style.backgroundColor = 'rgba(200, 0, 0, 0.8)';
        }
      }
    }
  }

  gameLog.render(document.getElementById('log') || document.body);
}

function setupEventListeners(): void {
  // Keyboard controls
  document.addEventListener('keydown', (e: KeyboardEvent) => {
    const key = e.key.toLowerCase();

    if (key === 'w' || key === 'arrowup') {
      e.preventDefault();
      movePlayer('up');
    } else if (key === 's' || key === 'arrowdown') {
      e.preventDefault();
      movePlayer('down');
    } else if (key === 'a' || key === 'arrowleft') {
      e.preventDefault();
      movePlayer('left');
    } else if (key === 'd' || key === 'arrowright') {
      e.preventDefault();
      movePlayer('right');
    } else if (key === ' ' || key === 'g') {
      e.preventDefault();
      pickupItem();
    }
  });

  // Canvas click for movement/attacks
  const canvas = document.getElementById('gameCanvas') as HTMLCanvasElement;
  if (canvas) {
    canvas.addEventListener('click', (e: MouseEvent) => {
      if (!gameInitialized || !store.state) return;

      const rect = canvas.getBoundingClientRect();
      const screenX = e.clientX - rect.left;
      const screenY = e.clientY - rect.top;

      const { x: worldX, y: worldY } = renderer.getWorldCoordinates(screenX, screenY);
      const { player } = store.state;

      // Check if it's an adjacent tile
      const dx = Math.abs(worldX - player.x);
      const dy = Math.abs(worldY - player.y);

      if (dx + dy !== 1) return; // Not adjacent

      // Check for enemy at this location
      const enemy = store.getEnemyAt(worldX, worldY);
      if (enemy) {
        attackEnemy(worldX, worldY);
        return;
      }

      // Check if it's walkable
      if (!store.isWall(worldX, worldY)) {
        if (worldX > player.x) movePlayer('right');
        else if (worldX < player.x) movePlayer('left');
        else if (worldY > player.y) movePlayer('down');
        else if (worldY < player.y) movePlayer('up');
      }
    });
  }

  // Inventory item use
  const hudContainer = document.getElementById('hud');
  if (hudContainer) {
    hudContainer.addEventListener('click', (e: MouseEvent) => {
      const target = e.target as HTMLElement;
      if (target.dataset.itemId) {
        useItem(target.dataset.itemId);
      }
    });
  }

  // New Game button
  const newGameBtn = document.getElementById('newGameBtn');
  if (newGameBtn) {
    newGameBtn.addEventListener('click', initializeGame);
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

  // Try to initialize on load
  initializeGame();
}

main();
