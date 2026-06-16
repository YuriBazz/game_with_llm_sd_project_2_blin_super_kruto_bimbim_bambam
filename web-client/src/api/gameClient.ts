import type {
    GameState,
    MapOptions,
    MapData,
    VisibleCell,
    MoveResponse,
    AttackResponse,
    UseItemResponse,
    VisibleCellsResponse,
    ActionsResponse,
    HealthResponse,
    ActionResponse,
} from '../types/game';

export class GameClient {
    private baseUrl: string;

    constructor(baseUrl: string = '') {
        this.baseUrl = baseUrl;
    }

    private async fetch<T>(
        endpoint: string,
        options?: RequestInit
    ): Promise<T> {
        const url = this.baseUrl + endpoint;
        const response = await fetch(url, options);

        if (!response.ok) {
            const text = await response.text();
            throw new Error(`HTTP ${response.status}: ${text}`);
        }

        return response.json() as Promise<T>;
    }

    async health(): Promise<HealthResponse> {
        return this.fetch<HealthResponse>('/health');
    }

    async newGame(options?: Partial<MapOptions>): Promise<GameState> {
        const body: MapOptions = {
            map_width: options?.map_width ?? 48,
            map_height: options?.map_height ?? 48,
            min_node_size: options?.min_node_size ?? 12,
            max_depth: options?.max_depth ?? 5,
            seed: options?.seed ?? 0,
            mode: options?.mode ?? 'start',
        };

        const response = await this.fetch<ActionResponse>('/api/map', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });

        if (!response.state) {
            throw new Error('No state in response');
        }
        return response.state;
    }

    async getMap(): Promise<MapData> {
        return this.fetch<MapData>('/api/map');
    }

    async getState(): Promise<GameState> {
        return this.fetch<GameState>('/api/state');
    }

    async tick(): Promise<GameState> {
        const response = await this.fetch<ActionResponse>('/api/tick', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}',
        });
        if (!response.state) {
            throw new Error('No state in tick response');
        }
        return response.state;
    }

    async move(direction: 'up' | 'down' | 'left' | 'right'): Promise<MoveResponse> {
        return this.fetch<MoveResponse>('/api/move', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ direction }),
        });
    }

    async attack(targetX: number, targetY: number): Promise<AttackResponse> {
        return this.fetch<AttackResponse>('/api/attack', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ target_x: targetX, target_y: targetY }),
        });
    }

    async useItem(itemId: string): Promise<UseItemResponse> {
        return this.fetch<UseItemResponse>('/api/use_item', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ item_id: itemId }),
        });
    }

    async pickupItem(): Promise<ActionResponse> {
        return this.fetch<ActionResponse>('/api/pickup_item', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{}',
        });
    }

    async getVisibleCells(radius: number = 5): Promise<VisibleCell[]> {
        const response = await this.fetch<VisibleCellsResponse>(
            `/api/visible_cells?radius=${Math.max(1, Math.min(10, radius))}`
        );
        return response.cells ?? [];
    }

    async getAvailableActions(): Promise<string[]> {
        const response = await this.fetch<ActionsResponse>('/api/available_actions');
        return response.actions ?? [];
    }
}