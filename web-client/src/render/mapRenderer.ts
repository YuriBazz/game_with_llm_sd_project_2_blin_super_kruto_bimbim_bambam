import type { GameStore } from '../state/gameStore';

const CELL_SIZE = 20;
const COLORS = {
  wall: '#2d2d3d',
  floor: '#3d3d4d',
  player: '#4da6ff',
  rival: '#00dd88',
  spectator: '#c77dff',
  enemy: '#ff4d4d',
  loot: '#ffcc00',
  visible: 'rgba(0, 0, 0, 0)',
  notVisible: 'rgba(0, 0, 0, 0.7)',
};

export class MapRenderer {
  canvas: HTMLCanvasElement;
  ctx: CanvasRenderingContext2D;
  private cameraX: number = 0;
  private cameraY: number = 0;

  constructor(canvasElement: HTMLCanvasElement) {
    this.canvas = canvasElement;
    const ctx = canvasElement.getContext('2d');
    if (!ctx) throw new Error('Failed to get canvas context');
    this.ctx = ctx;
  }

  setSize(width: number, height: number): void {
    const w = Math.max(1, Math.floor(width));
    const h = Math.max(1, Math.floor(height));
    if (this.canvas.width === w && this.canvas.height === h) return;
    this.canvas.width = w;
    this.canvas.height = h;
  }

  centerOn(focusX: number, focusY: number, mapWidth: number, mapHeight: number): void {
    const viewportWidth = this.canvas.width / CELL_SIZE;
    const viewportHeight = this.canvas.height / CELL_SIZE;

    let camX = focusX - viewportWidth / 2;
    let camY = focusY - viewportHeight / 2;

    if (mapWidth <= viewportWidth) {
      const minCamX = -(viewportWidth - mapWidth);
      camX = Math.max(minCamX, Math.min(camX, 0));
    } else {
      camX = Math.max(0, Math.min(camX, mapWidth - viewportWidth));
    }

    if (mapHeight <= viewportHeight) {
      const minCamY = -(viewportHeight - mapHeight);
      camY = Math.max(minCamY, Math.min(camY, 0));
    } else {
      camY = Math.max(0, Math.min(camY, mapHeight - viewportHeight));
    }

    this.cameraX = camX;
    this.cameraY = camY;
  }

  render(store: GameStore): void {
    if (!store.map || !store.state) return;

    const { width, height, grid } = store.map;
    const { player, rival, enemies, spectator, god_mode: godModeRaw } = store.state;
    const godMode = godModeRaw === true;
    const visibleSet = store.getVisibleCellsSet();
    const canSee = (x: number, y: number) => godMode || visibleSet.has(`${x},${y}`);

    if (godMode) {
      const sx = spectator?.x ?? Math.floor(width / 2);
      const sy = spectator?.y ?? Math.floor(height / 2);
      this.centerOn(sx, sy, width, height);
    } else {
      this.centerOn(player.x, player.y, width, height);
    }

    this.ctx.fillStyle = '#1a1a2e';
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const screenX = (x - this.cameraX) * CELL_SIZE;
        const screenY = (y - this.cameraY) * CELL_SIZE;

        if (
          screenX + CELL_SIZE < 0 ||
          screenX > this.canvas.width ||
          screenY + CELL_SIZE < 0 ||
          screenY > this.canvas.height
        ) {
          continue;
        }

        const idx = y * width + x;
        const isWall = grid[idx] === 0;
        const isVisible = canSee(x, y);

        this.ctx.fillStyle = isWall ? COLORS.wall : COLORS.floor;
        this.ctx.fillRect(screenX, screenY, CELL_SIZE, CELL_SIZE);

        this.ctx.strokeStyle = '#505060';
        this.ctx.lineWidth = 0.5;
        this.ctx.strokeRect(screenX, screenY, CELL_SIZE, CELL_SIZE);

        if (!isVisible) {
          this.ctx.fillStyle = COLORS.notVisible;
          this.ctx.fillRect(screenX, screenY, CELL_SIZE, CELL_SIZE);
        }
      }
    }

    for (const loot of store.state.map_loot) {
      if (!canSee(loot.x, loot.y)) continue;
      const screenX = (loot.x - this.cameraX) * CELL_SIZE;
      const screenY = (loot.y - this.cameraY) * CELL_SIZE;

      if (
        screenX + CELL_SIZE >= 0 &&
        screenX <= this.canvas.width &&
        screenY + CELL_SIZE >= 0 &&
        screenY <= this.canvas.height
      ) {
        this.ctx.fillStyle = COLORS.loot;
        if (loot.item?.id === 'extra_life') {
          this.ctx.fillStyle = '#ff66ff';
        }
        this.ctx.beginPath();
        this.ctx.arc(screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2, 4, 0, Math.PI * 2);
        this.ctx.fill();
      }
    }

    for (const enemy of enemies) {
      if (!canSee(enemy.x, enemy.y)) continue;
      const screenX = (enemy.x - this.cameraX) * CELL_SIZE;
      const screenY = (enemy.y - this.cameraY) * CELL_SIZE;

      if (
        screenX + CELL_SIZE >= 0 &&
        screenX <= this.canvas.width &&
        screenY + CELL_SIZE >= 0 &&
        screenY <= this.canvas.height
      ) {
        this.ctx.fillStyle = enemy.type === 'rat' ? '#c8a060' : COLORS.enemy;
        this.ctx.fillRect(screenX + 2, screenY + 2, CELL_SIZE - 4, CELL_SIZE - 4);

        this.ctx.fillStyle = '#1a1a2e';
        this.ctx.font = 'bold 10px monospace';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText(
          enemy.type[0].toUpperCase(),
          screenX + CELL_SIZE / 2,
          screenY + CELL_SIZE / 2
        );
      }
    }

    const drawActor = (
      x: number,
      y: number,
      label: string,
      fillColor: string,
      fontSize: string,
      alwaysVisible = false
    ): void => {
      if (!alwaysVisible && !canSee(x, y)) return;
      const screenX = (x - this.cameraX) * CELL_SIZE;
      const screenY = (y - this.cameraY) * CELL_SIZE;
      if (
        screenX + CELL_SIZE < 0 ||
        screenX > this.canvas.width ||
        screenY + CELL_SIZE < 0 ||
        screenY > this.canvas.height
      ) {
        return;
      }
      this.ctx.fillStyle = fillColor;
      this.ctx.fillRect(screenX + 2, screenY + 2, CELL_SIZE - 4, CELL_SIZE - 4);
      if (label === 'O') {
        this.ctx.strokeStyle = '#ffffff';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(screenX + 1, screenY + 1, CELL_SIZE - 2, CELL_SIZE - 2);
      }
      this.ctx.fillStyle = '#1a1a2e';
      this.ctx.font = fontSize;
      this.ctx.textAlign = 'center';
      this.ctx.textBaseline = 'middle';
      this.ctx.fillText(label, screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2);
    };

    if (rival && rival.hp > 0) {
      drawActor(rival.x, rival.y, 'A', COLORS.rival, 'bold 12px monospace');
    }

    drawActor(player.x, player.y, 'H', COLORS.player, 'bold 12px monospace');

    if (godMode && spectator) {
      drawActor(spectator.x, spectator.y, 'O', COLORS.spectator, 'bold 11px monospace', true);
    } else if (godMode && !spectator) {
      const sx = Math.floor(width / 2);
      const sy = Math.floor(height / 2);
      drawActor(sx, sy, 'O', COLORS.spectator, 'bold 11px monospace', true);
    }
  }

  getWorldCoordinates(screenX: number, screenY: number): { x: number; y: number } {
    return {
      x: Math.floor(screenX / CELL_SIZE) + this.cameraX,
      y: Math.floor(screenY / CELL_SIZE) + this.cameraY,
    };
  }
}
