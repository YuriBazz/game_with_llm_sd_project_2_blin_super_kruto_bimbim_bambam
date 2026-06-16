import { GameClient } from './api/gameClient';
import { GameStore } from './state/gameStore';
import { MapRenderer } from './render/mapRenderer';
import { GameLog, HUD, type AgentPanelData } from './ui/hud';
import { isVictory, isDefeat, isLevelComplete, type GameState } from './types/game';

let client: GameClient;
let store: GameStore;
let renderer: MapRenderer;
let gameLog: GameLog;
let hud: HUD;

let isInputLocked = false;
let gameInitialized = false;
let pollTimer: ReturnType<typeof setInterval> | null = null;

function isSpectatorMode(state: GameState | null): boolean {
  return state?.god_mode === true;
}

function hideStartScreen(): void {
  const el = document.getElementById('startOverlay');
  if (el) el.style.display = 'none';
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
  updateSpectatorBanner();
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

function buildAgentPanel(
  marker: 'H' | 'A',
  actor: { x: number; y: number; hp: number; max_hp: number; gold: number; inventory: AgentPanelData['inventory']; respawns_remaining?: number },
  kills: number,
  killsRequired: number,
  color: string,
  label: string
): AgentPanelData {
  return {
    marker,
    label,
    color,
    x: actor.x,
    y: actor.y,
    hp: actor.hp,
    maxHp: actor.max_hp,
    kills,
    killsRequired,
    respawns: actor.respawns_remaining ?? 0,
    gold: actor.gold,
    inventory: actor.inventory ?? [],
  };
}

function resizeCanvasToContainer(): void {
  const canvas = document.getElementById('gameCanvas') as HTMLCanvasElement | null;
  if (!canvas || !renderer) return;

  const rect = canvas.getBoundingClientRect();
  if (rect.width < 64 || rect.height < 64) {
    window.requestAnimationFrame(resizeCanvasToContainer);
    return;
  }

  const width = Math.floor(rect.width);
  const height = Math.floor(rect.height);
  renderer.setSize(width, height);
  if (gameInitialized) {
    render();
  }
}

async function moveSpectator(direction: 'up' | 'down' | 'left' | 'right'): Promise<void> {
  if (isInputLocked || !gameInitialized || !store.state) return;
  if (!isSpectatorMode(store.state)) return;

  try {
    isInputLocked = true;
    const response = await client.spectatorMove(direction);
    if (response.state) {
      store.updateState(response.state);
    }
    const pos = store.state?.spectator;
    if (pos) {
      gameLog.add(`Observer O → (${pos.x}, ${pos.y}) [${direction}]`);
    }
    render();
  } catch (error) {
    console.error('Spectator move failed:', error);
    gameLog.add(`Spectator move failed: ${error instanceof Error ? error.message : 'Error'}`);
  } finally {
    isInputLocked = false;
  }
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

    gameInitialized = true;
    isInputLocked = false;
    gameLog.clear();

    if (state.god_mode) {
      gameLog.add('Spectator mode — you are O. Robots H & A fight via LLM.');
      gameLog.add('WASD/Arrows move observer O (noclip, full map). H and A are independent robots.');
    } else {
      gameLog.add('WARNING: GODMODE=false — no spectator. Set GODMODE=true and restart stack.');
      gameLog.add('Camera follows H with fog of war. Use ./scripts/compose-up.sh --godmode');
    }

    resizeCanvasToContainer();

    startPolling();
    render();
    updateSpectatorBanner();
    updateControlsInfo();
  } catch (error) {
    console.error('Failed to initialize game:', error);
    gameLog.add(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
    if (!gameInitialized) showStartScreen();
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

function updateSpectatorBanner(): void {
  const banner = document.getElementById('godModeBanner');
  if (!banner) return;
  if (store.state?.god_mode) {
    banner.style.display = 'inline-block';
    banner.textContent = 'SPECTATOR — you are O, not H or A';
  } else {
    banner.style.display = 'none';
  }
}

function updateControlsInfo(): void {
  const info = document.getElementById('info');
  if (!info) return;
  if (isSpectatorMode(store.state)) {
    info.innerHTML = `
      <div><span>WASD</span> / Arrows — Move observer O (noclip)</div>
      <div><span>H</span> — Robot LLM &nbsp;|&nbsp; <span>A</span> — Robot LLM</div>
      <div>You observe only. H and A play against each other.</div>
      <div style="border-top: 1px solid #505060; margin-top: 8px; padding-top: 8px;">
        Purple O = you. Blue H / green A = neural networks.
      </div>
    `;
  }
}

function updateLevelCompleteOverlay(state: GameState): void {
  const msg = document.getElementById('levelCompleteMessage');
  if (!msg) return;

  if (state.level_winner === 'human') {
    msg.textContent = 'Level complete! Robot H reached kill quota first.';
  } else if (state.level_winner === 'ai') {
    msg.textContent = 'Level complete! Robot A reached kill quota first.';
  } else {
    msg.textContent = 'Level complete!';
  }
}

function render(): void {
  if (!store.state || !store.map) return;

  const { player, rival, spectator } = store.state;
  const killsRequired = store.state.kills_required ?? 4;
  const meta = {
    steps: store.state.steps,
    level: store.state.campaign_level ?? 1,
  };

  renderer.render(store);

  if (isSpectatorMode(store.state) && rival) {
    hud.renderSpectator(
      { x: spectator?.x ?? 0, y: spectator?.y ?? 0 },
      buildAgentPanel(
        'H',
        player,
        store.state.player_kills ?? 0,
        killsRequired,
        '#4da6ff',
        'Robot H (LLM)'
      ),
      buildAgentPanel(
        'A',
        rival,
        store.state.rival_kills ?? 0,
        killsRequired,
        '#00dd88',
        'Robot A (LLM)'
      ),
      meta
    );
  } else if (rival) {
    hud.renderDualAgents(
      null,
      buildAgentPanel(
        'H',
        player,
        store.state.player_kills ?? 0,
        killsRequired,
        '#4da6ff',
        'Robot H'
      ),
      buildAgentPanel(
        'A',
        rival,
        store.state.rival_kills ?? 0,
        killsRequired,
        '#00dd88',
        'Robot A'
      ),
      meta
    );
  } else {
    hud.render(
      player.hp,
      player.max_hp,
      player.gold,
      '',
      store.state.steps,
      player.inventory,
      {
        level: store.state.campaign_level ?? 1,
        playerKills: store.state.player_kills ?? 0,
        rivalKills: store.state.rival_kills ?? 0,
        killsRequired: store.state.kills_required ?? 4,
      },
      { respawns: player.respawns_remaining ?? 0, x: player.x, y: player.y },
      undefined
    );
  }

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
  updateSpectatorBanner();
}

function setupEventListeners(): void {
  document.addEventListener('keydown', (e: KeyboardEvent) => {
    if (e.repeat) return;

    switch (e.code) {
      case 'KeyW':
      case 'ArrowUp':
        e.preventDefault();
        if (isSpectatorMode(store.state)) moveSpectator('up');
        break;
      case 'KeyS':
      case 'ArrowDown':
        e.preventDefault();
        if (isSpectatorMode(store.state)) moveSpectator('down');
        break;
      case 'KeyA':
      case 'ArrowLeft':
        e.preventDefault();
        if (isSpectatorMode(store.state)) moveSpectator('left');
        break;
      case 'KeyD':
      case 'ArrowRight':
        e.preventDefault();
        if (isSpectatorMode(store.state)) moveSpectator('right');
        break;
      default:
        break;
    }
  });

  const startBtn = document.getElementById('startGameBtn');
  if (startBtn) {
    startBtn.addEventListener('click', () => startGame('start'));
  }

  const startHeaderBtn = document.getElementById('startGameHeaderBtn');
  if (startHeaderBtn) {
    startHeaderBtn.addEventListener('click', () => startGame('start'));
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

async function bootstrap(): Promise<void> {
  client = new GameClient('');
  store = new GameStore();
  renderer = new MapRenderer(document.getElementById('gameCanvas') as HTMLCanvasElement);
  gameLog = new GameLog();
  hud = new HUD(document.getElementById('hud') as HTMLElement);

  setupEventListeners();

  const container = document.getElementById('gameContainer');
  if (container && typeof ResizeObserver !== 'undefined') {
    const observer = new ResizeObserver(() => resizeCanvasToContainer());
    observer.observe(container);
  }
  window.addEventListener('resize', resizeCanvasToContainer);
  resizeCanvasToContainer();

  try {
    const health = await client.health();
    if (health.god_mode) {
      gameLog.add('Press Start Game to begin. Robots H & A wait for a new session.');
    }
    showStartScreen();
  } catch {
    showStartScreen();
  }
}

bootstrap();
