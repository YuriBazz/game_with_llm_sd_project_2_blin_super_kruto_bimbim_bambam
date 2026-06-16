import type { GameState, MapData, VisibleCell } from '../types/game';

export class GameStore {
    state: GameState | null = null;
    map: MapData | null = null;
    visibleCells: VisibleCell[] = [];

    updateState(state: GameState): void {
        this.state = state;
    }

    updateMap(map: MapData): void {
        this.map = map;
    }

    updateVisibleCells(cells: VisibleCell[]): void {
        this.visibleCells = cells;
    }

    getVisibleCellsSet(): Set<string> {
        const set = new Set<string>();
        for (const cell of this.visibleCells) {
            set.add(`${cell.x},${cell.y}`);
        }
        return set;
    }

    isWall(x: number, y: number): boolean {
        if (!this.map) return false;
        const { width, grid } = this.map;
        if (x < 0 || x >= this.map.width || y < 0 || y >= this.map.height) {
            return true;
        }
        const idx = y * width + x;
        return grid[idx] === 0;
    }

    isFloor(x: number, y: number): boolean {
        return !this.isWall(x, y);
    }

    getEnemyAt(x: number, y: number) {
        return this.state?.enemies.find(e => e.x === x && e.y === y) ?? null;
    }

    getLootAt(x: number, y: number) {
        return this.state?.map_loot.find(l => l.x === x && l.y === y) ?? null;
    }
}