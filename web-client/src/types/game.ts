export interface Item {
    id: string;
    name: string;
    type: string;
    count: number;
}

export interface Enemy {
    id: number;
    x: number;
    y: number;
    hp: number;
    type: string;
}

export interface Player {
    x: number;
    y: number;
    hp: number;
    max_hp: number;
    gold: number;
    inventory: Item[];
    respawns_remaining?: number;
    spawn_x?: number;
    spawn_y?: number;
}

export interface Loot {
    id: number;
    x: number;
    y: number;
    item: Item;
}

export interface GameState {
    enemies: Enemy[];
    player: Player;
    rival?: Player;
    session_active?: boolean;
    session_id?: number;
    game_state?: 'running' | 'player_victory' | 'enemy_victory' | 'level_complete';
    game_over: boolean;
    phase: 'player_turn' | 'enemy_turn' | 'player_dead' | 'victory' | 'realtime' | 'level_complete';
    won: boolean;
    level_complete?: boolean;
    campaign_level?: number;
    player_kills?: number;
    rival_kills?: number;
    kills_required?: number;
    max_enemies_on_level?: number;
    level_winner?: 'none' | 'human' | 'ai';
    god_mode?: boolean;
    steps: number;
    map_loot: Loot[];
}

export function isPlayable(state: GameState): boolean {
    if (isLevelComplete(state)) return false;
    if (state.god_mode) return state.game_state === 'running' || state.phase === 'realtime';
    if (state.game_over || state.won) return false;
    if (state.phase === 'realtime' || state.phase === 'player_turn') return true;
    return state.game_state === 'running';
  enemies: Enemy[];
  player: Player;
  rival?: Player;
  session_active?: boolean;
  session_id?: number;
  game_state?: 'running' | 'player_victory' | 'enemy_victory' | 'level_complete';
  game_over: boolean;
  phase: 'player_turn' | 'enemy_turn' | 'player_dead' | 'victory' | 'realtime' | 'level_complete';
  won: boolean;
  level_complete?: boolean;
  campaign_level?: number;
  player_kills?: number;
  rival_kills?: number;
  kills_required?: number;
  max_enemies_on_level?: number;
  level_winner?: 'none' | 'human' | 'ai';
  god_mode?: boolean;
  spectator?: { x: number; y: number };
  steps: number;
  map_loot: Loot[];
}

export function isPlayable(state: GameState): boolean {
  if (state.god_mode) return false;
  if (isLevelComplete(state)) return false;
  if (state.game_over || state.won) return false;
  if (state.phase === 'realtime' || state.phase === 'player_turn') return true;
  return state.game_state === 'running';
}

export function isLevelComplete(state: GameState): boolean {
    return state.level_complete === true ||
        state.game_state === 'level_complete' ||
        state.phase === 'level_complete';
}

export function isVictory(state: GameState): boolean {
    return state.won || state.game_state === 'player_victory' || state.phase === 'victory';
}

export function isDefeat(state: GameState): boolean {
    return (state.game_over && !state.won) ||
        state.game_state === 'enemy_victory' ||
        state.phase === 'player_dead';
}

export interface MapOptions {
    map_width: number;
    map_height: number;
    min_node_size: number;
    max_depth: number;
    seed: number;
    mode?: 'start' | 'reset' | 'next_level';
}

export interface Room {
    x: number;
    y: number;
    w: number;
    h: number;
}

export interface Corridor {
    x1: number;
    y1: number;
    x2: number;
    y2: number;
}

export interface MapData {
    width: number;
    height: number;
    seed: number;
    rooms: Room[];
    corridors: Corridor[];
    grid: number[];
}

export interface VisibleCell {
    x: number;
    y: number;
    type: 'wall' | 'floor';
}

export interface ActionResponse {
    success: boolean;
    state?: GameState;
    [key: string]: unknown;
}

export interface MoveResponse extends ActionResponse {
    x?: number;
    y?: number;
}

export interface AttackResponse extends ActionResponse {
    damage?: number;
    target_dead?: boolean;
}

export interface PickupResponse extends ActionResponse {
    items?: Item[];
}

export interface UseItemResponse extends ActionResponse {
    effect?: string;
    value?: number;
}

export interface VisibleCellsResponse extends ActionResponse {
    cells?: VisibleCell[];
}

export interface ActionsResponse extends ActionResponse {
    actions?: string[];
}

export interface HealthResponse {
    status: string;
    service: string;
    god_mode?: boolean;
}