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
  game_over: boolean;
  phase: 'player_turn' | 'enemy_turn' | 'player_dead' | 'victory';
  won: boolean;
  steps: number;
  map_loot: Loot[];
}

export interface MapOptions {
  map_width: number;
  map_height: number;
  min_node_size: number;
  max_depth: number;
  seed: number;
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
}
