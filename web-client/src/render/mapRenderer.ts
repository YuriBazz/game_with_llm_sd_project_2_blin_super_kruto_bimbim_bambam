import type { GameStore } from '../state/gameStore';

const CELL_SIZE = 20;
const COLORS = {
  wall: '#2d2d3d',
  floor: '#3d3d4d',
  player: '#4da6ff',
  player_god: '#c77dff',
  rival: '#00dd88',
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
  private freeCamera: boolean = false;

  constructor(canvasElement: HTMLCanvasElement) {
    this.canvas = canvasElement;
    const ctx = canvasElement.getContext('2d');
    if (!ctx) throw new Error('Failed to get canvas context');
    this.ctx = ctx;
  }

  setSize(width: number, height: number): void {
    this.canvas.width = width;
    this.canvas.height = height;
  }

  setFreeCamera(enabled: boolean, anchorX?: number, anchorY?: number): void {
    this.freeCamera = enabled;
    if (enabled && anchorX !== undefined && anchorY !== undefined) {
      this.centerOn(anchorX, anchorY);
    }
  }

  centerOn(x: number, y: number): void {
    const viewportWidth = this.canvas.width / CELL_SIZE;
    const viewportHeight = this.canvas.height / CELL_SIZE;
    this.cameraX = Math.max(0, x - Math.floor(viewportWidth / 2));
    this.cameraY = Math.max(0, y - Math.floor(viewportHeight / 2));
  }

  updateCamera(playerX: number, playerY: number): void {
    if (!this.freeCamera) {
      this.centerOn(playerX, playerY);
    }
  }

  panCamera(dxCells: number, dyCells: number, mapWidth: number, mapHeight: number): void {
    this.cameraX = Math.max(0, Math.min(mapWidth - 1, this.cameraX + dxCells));
    this.cameraY = Math.max(0, Math.min(mapHeight - 1, this.cameraY + dyCells));
  }

  render(store: GameStore): void {
    if (!store.map || !store.state) return;

    const { width, height, grid } = store.map;
    const { player, rival, enemies } = store.state;
    const godMode = store.state.god_mode === true;
    const visibleSet = store.getVisibleCellsSet();
    const canSee = (x: number, y: number) => godMode || visibleSet.has(`${x},${y}`);

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
        this.ctx.fillStyle = COLORS.enemy;
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
      fontSize: string
    ): void => {
      if (!canSee(x, y)) return;
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
      this.ctx.fillStyle = '#1a1a2e';
      this.ctx.font = fontSize;
      this.ctx.textAlign = 'center';
      this.ctx.textBaseline = 'middle';
      this.ctx.fillText(label, screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2);
    };

    if (rival && rival.hp > 0) {
      drawActor(rival.x, rival.y, 'A', COLORS.rival, 'bold 12px monospace');
    }

    drawActor(
      player.x,
      player.y,
      'H',
      godMode ? COLORS.player_god : COLORS.player,
      'bold 12px monospace'
    );
  }

  getWorldCoordinates(screenX: number, screenY: number): { x: number; y: number } {
    return {
      x: Math.floor(screenX / CELL_SIZE) + this.cameraX,
      y: Math.floor(screenY / CELL_SIZE) + this.cameraY,
    };
  }
}
