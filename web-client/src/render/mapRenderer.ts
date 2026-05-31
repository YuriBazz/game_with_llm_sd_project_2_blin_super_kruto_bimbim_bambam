import type { GameStore } from '../state/gameStore';

const CELL_SIZE = 20;
const COLORS = {
  wall: '#2d2d3d',
  floor: '#3d3d4d',
  player: '#4da6ff',
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
    this.canvas.width = width;
    this.canvas.height = height;
  }

  updateCamera(playerX: number, playerY: number): void {
    const viewportWidth = this.canvas.width / CELL_SIZE;
    const viewportHeight = this.canvas.height / CELL_SIZE;
    this.cameraX = Math.max(0, playerX - Math.floor(viewportWidth / 2));
    this.cameraY = Math.max(0, playerY - Math.floor(viewportHeight / 2));
  }

  render(store: GameStore): void {
    if (!store.map || !store.state) return;

    const { width, height, grid } = store.map;
    const { player, enemies } = store.state;
    const visibleSet = store.getVisibleCellsSet();

    this.ctx.fillStyle = '#1a1a2e';
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    // Draw map tiles
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
        const isVisible = visibleSet.has(`${x},${y}`);

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

    // Draw loot
    for (const loot of store.state.map_loot) {
      const screenX = (loot.x - this.cameraX) * CELL_SIZE;
      const screenY = (loot.y - this.cameraY) * CELL_SIZE;

      if (
        screenX + CELL_SIZE >= 0 &&
        screenX <= this.canvas.width &&
        screenY + CELL_SIZE >= 0 &&
        screenY <= this.canvas.height
      ) {
        this.ctx.fillStyle = COLORS.loot;
        this.ctx.beginPath();
        this.ctx.arc(screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2, 4, 0, Math.PI * 2);
        this.ctx.fill();
      }
    }

    // Draw enemies
    for (const enemy of enemies) {
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

        // Enemy type icon
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

    // Draw player
    const playerScreenX = (player.x - this.cameraX) * CELL_SIZE;
    const playerScreenY = (player.y - this.cameraY) * CELL_SIZE;

    this.ctx.fillStyle = COLORS.player;
    this.ctx.fillRect(playerScreenX + 2, playerScreenY + 2, CELL_SIZE - 4, CELL_SIZE - 4);

    this.ctx.fillStyle = '#1a1a2e';
    this.ctx.font = 'bold 12px monospace';
    this.ctx.textAlign = 'center';
    this.ctx.textBaseline = 'middle';
    this.ctx.fillText('@', playerScreenX + CELL_SIZE / 2, playerScreenY + CELL_SIZE / 2);
  }

  getWorldCoordinates(screenX: number, screenY: number): { x: number; y: number } {
    return {
      x: Math.floor(screenX / CELL_SIZE) + this.cameraX,
      y: Math.floor(screenY / CELL_SIZE) + this.cameraY,
    };
  }
}
