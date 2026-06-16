import type { GameStore } from '../state/gameStore';

const CELL_SIZE = 20;

const COLORS = {
    wall: '#2d2d3d',
    floor: '#3d3d4d',
    player: '#c77d00',
    player_god: '#ffaa33',
    rival: '#44aaff',
    enemy_goblin: '#44dd44',
    enemy_orc: '#dd6644',
    enemy_troll: '#aa66dd',
    enemy_rat: '#888888',
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
    private animationFrame: number = 0;
    private lastAnimTime: number = 0;

    constructor(canvasElement: HTMLCanvasElement) {
        this.canvas = canvasElement;
        const ctx = canvasElement.getContext('2d');
        if (!ctx) throw new Error('Failed to get canvas context');
        this.ctx = ctx;
    }

    private lightenColor(color: string, percent: number): string {
        const num = parseInt(color.replace('#', ''), 16);
        const amt = Math.round(2.55 * percent);
        const R = Math.min(255, (num >> 16) + amt);
        const G = Math.min(255, ((num >> 8) & 0x00FF) + amt);
        const B = Math.min(255, (num & 0x0000FF) + amt);
        return `#${(1 << 24 | R << 16 | G << 8 | B).toString(16).slice(1)}`;
    }

    private darkenColor(color: string, percent: number): string {
        const num = parseInt(color.replace('#', ''), 16);
        const amt = Math.round(2.55 * percent);
        const R = Math.max(0, (num >> 16) - amt);
        const G = Math.max(0, ((num >> 8) & 0x00FF) - amt);
        const B = Math.max(0, (num & 0x0000FF) - amt);
        return `#${(1 << 24 | R << 16 | G << 8 | B).toString(16).slice(1)}`;
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

        const now = Date.now();
        if (now - this.lastAnimTime > 160) {
            this.animationFrame = (this.animationFrame + 1) % 2;
            this.lastAnimTime = now;
        }

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

                if (screenX + CELL_SIZE < 0 || screenX > this.canvas.width ||
                    screenY + CELL_SIZE < 0 || screenY > this.canvas.height) {
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

            if (screenX + CELL_SIZE >= 0 && screenX <= this.canvas.width &&
                screenY + CELL_SIZE >= 0 && screenY <= this.canvas.height) {
                this.ctx.fillStyle = loot.item?.id === 'extra_life' ? '#ff66ff' : COLORS.loot;
                this.ctx.beginPath();
                this.ctx.arc(screenX + CELL_SIZE / 2, screenY + CELL_SIZE / 2, 4, 0, Math.PI * 2);
                this.ctx.fill();
            }
        }

        for (const enemy of enemies) {
            if (!canSee(enemy.x, enemy.y)) continue;
            const type = enemy.type.toLowerCase();
            let color = COLORS.enemy_goblin;
            if (type === 'orc') color = COLORS.enemy_orc;
            else if (type === 'troll') color = COLORS.enemy_troll;
            else if (type === 'rat') color = COLORS.enemy_rat;
            this.drawEnemySprite(enemy.x, enemy.y, enemy.type, color);
        }

        if (rival && rival.hp > 0) {
            this.drawRobotSprite(rival.x, rival.y);
        }

        this.drawHeroSprite(
            player.x, player.y,
            godMode ? COLORS.player_god : COLORS.player
        );
    }

    private drawHeroSprite(x: number, y: number, tintColor: string): void {
        const screenX = (x - this.cameraX) * CELL_SIZE;
        const screenY = (y - this.cameraY) * CELL_SIZE;

        if (screenX < -CELL_SIZE || screenX > this.canvas.width ||
            screenY < -CELL_SIZE || screenY > this.canvas.height) return;

        const size = CELL_SIZE - 4;
        const offset = 2;
        const cx = screenX + offset + size / 2;
        const cy = screenY + offset + size / 2;

        this.ctx.shadowColor = 'rgba(0,0,0,0.3)';
        this.ctx.shadowBlur = 4;

        const gradient = this.ctx.createRadialGradient(
            cx - 2, cy - 2, 2,
            cx, cy, size / 2
        );
        gradient.addColorStop(0, this.lightenColor(tintColor, 50));
        gradient.addColorStop(1, tintColor);

        this.ctx.shadowBlur = 0;

        this.ctx.fillStyle = gradient;
        this.ctx.beginPath();
        this.ctx.arc(cx, cy + 2, size / 2 - 1, 0, Math.PI * 2);
        this.ctx.fill();

        this.ctx.strokeStyle = this.darkenColor(tintColor, 30);
        this.ctx.lineWidth = 1.5;
        this.ctx.beginPath();
        this.ctx.arc(cx, cy + 2, size / 2 - 1, 0, Math.PI * 2);
        this.ctx.stroke();

        this.ctx.fillStyle = '#f5d0b8';
        this.ctx.beginPath();
        this.ctx.arc(cx, cy + 2, size / 2 - 4, 0, Math.PI * 2);
        this.ctx.fill();

        this.ctx.fillStyle = '#1a1a2e';
        this.ctx.beginPath();
        this.ctx.arc(cx - 3, cy, 1.8, 0, Math.PI * 2);
        this.ctx.fill();
        this.ctx.beginPath();
        this.ctx.arc(cx + 3, cy, 1.8, 0, Math.PI * 2);
        this.ctx.fill();

        const eyeOffset = this.animationFrame === 0 ? 0 : 0.5;
        this.ctx.fillStyle = '#ffffff';
        this.ctx.beginPath();
        this.ctx.arc(cx - 3 + eyeOffset, cy - 0.5, 0.8, 0, Math.PI * 2);
        this.ctx.fill();
        this.ctx.beginPath();
        this.ctx.arc(cx + 3 + eyeOffset, cy - 0.5, 0.8, 0, Math.PI * 2);
        this.ctx.fill();

        this.ctx.strokeStyle = '#1a1a2e';
        this.ctx.lineWidth = 1;
        this.ctx.beginPath();
        this.ctx.arc(cx, cy + 3, 2.5, 0.1 * Math.PI, 0.9 * Math.PI);
        this.ctx.stroke();

        this.ctx.fillStyle = tintColor;
        this.ctx.fillRect(cx - 5, cy + 5, 1.5, 4);
        this.ctx.fillRect(cx + 3.5, cy + 5, 1.5, 4);

        this.ctx.fillStyle = this.darkenColor(tintColor, 40);
        this.ctx.fillRect(cx - 6, cy + 8, 3, 1);
        this.ctx.fillRect(cx + 3, cy + 8, 3, 1);

        this.ctx.fillStyle = '#c77d00';
        this.ctx.fillRect(cx - 3, cy - 6, 6, 2);
        this.ctx.fillStyle = this.lightenColor('#c77d00', 30);
        this.ctx.fillRect(cx - 1, cy - 8, 2, 3);

        this.ctx.fillStyle = 'rgba(255,255,255,0.6)';
        this.ctx.font = 'bold 6px monospace';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'bottom';
        this.ctx.shadowColor = 'rgba(0,0,0,0.8)';
        this.ctx.shadowBlur = 2;
        this.ctx.fillText('H', cx, screenY + offset - 1);
        this.ctx.shadowBlur = 0;
    }

    private drawRobotSprite(x: number, y: number): void {
        const screenX = (x - this.cameraX) * CELL_SIZE;
        const screenY = (y - this.cameraY) * CELL_SIZE;

        if (screenX < -CELL_SIZE || screenX > this.canvas.width ||
            screenY < -CELL_SIZE || screenY > this.canvas.height) return;

        const size = CELL_SIZE - 4;
        const offset = 2;
        const cx = screenX + offset + size / 2;
        const cy = screenY + offset + size / 2;

        this.ctx.shadowColor = 'rgba(0,0,0,0.3)';
        this.ctx.shadowBlur = 4;
        this.ctx.shadowBlur = 0;

        this.ctx.fillStyle = '#88bbdd';
        this.ctx.fillRect(cx - 4, cy - 6, 8, 6);

        this.ctx.strokeStyle = '#446688';
        this.ctx.lineWidth = 1;
        this.ctx.strokeRect(cx - 4, cy - 6, 8, 6);

        this.ctx.fillStyle = '#aaccee';
        this.ctx.fillRect(cx - 3, cy - 5, 6, 1);
        this.ctx.fillRect(cx - 3, cy - 3, 6, 1);

        this.ctx.fillStyle = '#446688';
        this.ctx.fillRect(cx - 3, cy - 4, 1.5, 1);
        this.ctx.fillRect(cx + 1.5, cy - 4, 1.5, 1);

        const eyeOffset = this.animationFrame === 0 ? 0 : 0.5;
        this.ctx.fillStyle = '#00ddff';
        this.ctx.shadowColor = '#00ddff';
        this.ctx.shadowBlur = 4;
        this.ctx.fillRect(cx - 3 + eyeOffset, cy - 5, 2, 2);
        this.ctx.fillRect(cx + 1 + eyeOffset, cy - 5, 2, 2);
        this.ctx.shadowBlur = 0;

        this.ctx.fillStyle = '#88bbdd';
        this.ctx.fillRect(cx - 5, cy + 1, 10, 6);

        this.ctx.strokeStyle = '#446688';
        this.ctx.lineWidth = 1;
        this.ctx.strokeRect(cx - 5, cy + 1, 10, 6);

        this.ctx.fillStyle = '#aaccee';
        this.ctx.fillRect(cx - 4, cy + 2, 8, 1);
        this.ctx.fillRect(cx - 4, cy + 4, 8, 1);

        this.ctx.fillStyle = '#446688';
        this.ctx.fillRect(cx - 2, cy + 1, 1, 6);
        this.ctx.fillRect(cx + 1, cy + 1, 1, 6);

        this.ctx.fillStyle = '#667788';
        this.ctx.fillRect(cx - 6, cy + 6, 2.5, 3);
        this.ctx.fillRect(cx + 3.5, cy + 6, 2.5, 3);

        this.ctx.fillStyle = 'rgba(255,255,255,0.6)';
        this.ctx.font = 'bold 6px monospace';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'bottom';
        this.ctx.shadowColor = 'rgba(0,0,0,0.8)';
        this.ctx.shadowBlur = 2;
        this.ctx.fillText('A', cx, screenY + offset - 1);
        this.ctx.shadowBlur = 0;
    }

    private drawEnemySprite(x: number, y: number, type: string, _tintColor: string): void {
        const screenX = (x - this.cameraX) * CELL_SIZE;
        const screenY = (y - this.cameraY) * CELL_SIZE;

        if (screenX < -CELL_SIZE || screenX > this.canvas.width ||
            screenY < -CELL_SIZE || screenY > this.canvas.height) return;

        const size = CELL_SIZE - 4;
        const offset = 2;
        const cx = screenX + offset + size / 2;
        const cy = screenY + offset + size / 2;

        this.ctx.shadowColor = 'rgba(0,0,0,0.3)';
        this.ctx.shadowBlur = 4;
        this.ctx.shadowBlur = 0;

        const label = type.charAt(0).toUpperCase();

        if (type.toLowerCase() === 'goblin') {
            this.ctx.fillStyle = '#55aa55';
            this.ctx.beginPath();
            this.ctx.ellipse(cx, cy + 1, 5, 6, 0, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#44aa44';
            this.ctx.beginPath();
            this.ctx.ellipse(cx - 3, cy - 2, 2.5, 3, -0.3, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.ellipse(cx + 3, cy - 2, 2.5, 3, 0.3, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#ff3333';
            this.ctx.beginPath();
            this.ctx.arc(cx - 2, cy - 1, 1.2, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 2, cy - 1, 1.2, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.strokeStyle = '#338833';
            this.ctx.lineWidth = 1;
            this.ctx.beginPath();
            this.ctx.moveTo(cx - 2, cy + 2);
            this.ctx.quadraticCurveTo(cx, cy + 4, cx + 2, cy + 2);
            this.ctx.stroke();

            this.ctx.fillStyle = '#338833';
            this.ctx.fillRect(cx - 4, cy + 4, 1.5, 2.5);
            this.ctx.fillRect(cx + 2.5, cy + 4, 1.5, 2.5);

            this.ctx.fillStyle = '#66cc66';
            this.ctx.beginPath();
            this.ctx.moveTo(cx - 3, cy - 6);
            this.ctx.lineTo(cx - 1, cy - 3);
            this.ctx.lineTo(cx - 5, cy - 4);
            this.ctx.fill();

            this.ctx.beginPath();
            this.ctx.moveTo(cx + 3, cy - 6);
            this.ctx.lineTo(cx + 1, cy - 3);
            this.ctx.lineTo(cx + 5, cy - 4);
            this.ctx.fill();
        } else if (type.toLowerCase() === 'orc') {
            this.ctx.fillStyle = '#cc8844';
            this.ctx.beginPath();
            this.ctx.ellipse(cx, cy + 1, 6, 7, 0, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#bb7733';
            this.ctx.fillRect(cx - 2, cy - 4, 4, 2);

            this.ctx.fillStyle = '#ff4444';
            this.ctx.beginPath();
            this.ctx.arc(cx - 3, cy - 1, 1.5, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 3, cy - 1, 1.5, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#222222';
            this.ctx.beginPath();
            this.ctx.arc(cx - 3, cy - 0.5, 0.8, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 3, cy - 0.5, 0.8, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.strokeStyle = '#886633';
            this.ctx.lineWidth = 1.5;
            this.ctx.beginPath();
            this.ctx.moveTo(cx - 4, cy + 2);
            this.ctx.lineTo(cx + 4, cy + 2);
            this.ctx.stroke();

            this.ctx.strokeStyle = '#886633';
            this.ctx.lineWidth = 1;
            this.ctx.beginPath();
            this.ctx.moveTo(cx - 6, cy + 3);
            this.ctx.lineTo(cx + 6, cy + 3);
            this.ctx.stroke();

            this.ctx.fillStyle = '#aa7733';
            this.ctx.fillRect(cx - 5, cy + 5, 2, 3);
            this.ctx.fillRect(cx + 3, cy + 5, 2, 3);
        } else if (type.toLowerCase() === 'troll') {
            this.ctx.fillStyle = '#9966bb';
            this.ctx.beginPath();
            this.ctx.ellipse(cx, cy + 1, 7, 6, 0, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#bb88dd';
            this.ctx.fillRect(cx - 4, cy - 3, 8, 2);

            this.ctx.fillStyle = '#ff8800';
            this.ctx.beginPath();
            this.ctx.arc(cx - 3, cy - 1, 1.8, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 3, cy - 1, 1.8, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#222222';
            this.ctx.beginPath();
            this.ctx.arc(cx - 3, cy - 0.5, 0.8, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 3, cy - 0.5, 0.8, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.strokeStyle = '#8844aa';
            this.ctx.lineWidth = 1.5;
            this.ctx.beginPath();
            this.ctx.arc(cx, cy + 3, 3, 0.1 * Math.PI, 0.9 * Math.PI);
            this.ctx.stroke();

            this.ctx.fillStyle = '#8844aa';
            this.ctx.fillRect(cx - 5, cy + 4, 1.5, 3);
            this.ctx.fillRect(cx + 3.5, cy + 4, 1.5, 3);

            this.ctx.fillStyle = '#aa77cc';
            this.ctx.beginPath();
            this.ctx.ellipse(cx - 5, cy - 4, 2, 3, -0.5, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.ellipse(cx + 5, cy - 4, 2, 3, 0.5, 0, Math.PI * 2);
            this.ctx.fill();
        } else if (type.toLowerCase() === 'rat') {
            this.ctx.fillStyle = '#888888';
            this.ctx.beginPath();
            this.ctx.ellipse(cx, cy + 2, 5, 4, 0, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#666666';
            this.ctx.beginPath();
            this.ctx.ellipse(cx - 3, cy - 1, 2.5, 2, -0.3, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.ellipse(cx + 3, cy - 1, 2.5, 2, 0.3, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#ff3333';
            this.ctx.beginPath();
            this.ctx.arc(cx - 2, cy, 1.2, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 2, cy, 1.2, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#222222';
            this.ctx.beginPath();
            this.ctx.arc(cx - 2, cy + 0.5, 0.6, 0, Math.PI * 2);
            this.ctx.fill();
            this.ctx.beginPath();
            this.ctx.arc(cx + 2, cy + 0.5, 0.6, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#999999';
            this.ctx.beginPath();
            this.ctx.ellipse(cx, cy - 4, 2, 1.5, 0, 0, Math.PI * 2);
            this.ctx.fill();

            this.ctx.fillStyle = '#777777';
            this.ctx.fillRect(cx - 4, cy + 3, 1.5, 2.5);
            this.ctx.fillRect(cx + 2.5, cy + 3, 1.5, 2.5);

            this.ctx.strokeStyle = '#666666';
            this.ctx.lineWidth = 0.8;
            this.ctx.beginPath();
            this.ctx.moveTo(cx + 4, cy - 5);
            this.ctx.quadraticCurveTo(cx + 7, cy - 7, cx + 6, cy - 4);
            this.ctx.stroke();
            this.ctx.beginPath();
            this.ctx.moveTo(cx - 4, cy - 5);
            this.ctx.quadraticCurveTo(cx - 7, cy - 7, cx - 6, cy - 4);
            this.ctx.stroke();
        }

        this.ctx.fillStyle = 'rgba(255,255,255,0.6)';
        this.ctx.font = 'bold 5px monospace';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'bottom';
        this.ctx.shadowColor = 'rgba(0,0,0,0.8)';
        this.ctx.shadowBlur = 2;
        this.ctx.fillText(label, cx, screenY + offset - 1);
        this.ctx.shadowBlur = 0;
    }

    getWorldCoordinates(screenX: number, screenY: number): { x: number; y: number } {
        return {
            x: Math.floor(screenX / CELL_SIZE) + this.cameraX,
            y: Math.floor(screenY / CELL_SIZE) + this.cameraY,
        };
    }
}