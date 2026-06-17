#!/usr/bin/env python3
"""
Example: Enemy Team Agent using Real-Time Game API

This demonstrates how an Enemy Team LLM Agent can control all enemies
"""

import requests
import json
import time
import math
from typing import List, Optional, Tuple

class EnemyTeamAgent:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
    
    def get_state(self) -> dict:
        """Get full game state"""
        resp = requests.get(f"{self.base_url}/api/state")
        return resp.json()
    
    def get_enemies(self) -> dict:
        """Get list of all enemies"""
        resp = requests.get(f"{self.base_url}/api/enemies")
        return resp.json()
    
    def get_enemy_visible_cells(self, enemy_index: int, radius: int = 6) -> dict:
        """Get cells visible to specific enemy"""
        resp = requests.get(
            f"{self.base_url}/api/enemy/visible_cells",
            params={"enemy_index": enemy_index, "radius": radius}
        )
        return resp.json()
    
    def enemy_move(self, enemy_index: int, direction: str) -> dict:
        """Move enemy in direction: up|down|left|right"""
        resp = requests.post(
            f"{self.base_url}/api/enemy/move",
            json={"enemy_index": enemy_index, "direction": direction}
        )
        return resp.json()
    
    def enemy_attack(self, enemy_index: int, target_x: int, target_y: int) -> dict:
        """Attack at coordinates"""
        resp = requests.post(
            f"{self.base_url}/api/enemy/attack",
            json={
                "enemy_index": enemy_index,
                "target_x": target_x,
                "target_y": target_y
            }
        )
        return resp.json()
    
    def simple_chase_loop(self, max_iterations: int = 100):
        """Simple chase AI for all enemies"""
        
        for i in range(max_iterations):
            state = self.get_state()
            game_state = state.get("state", {}).get("game_state", "running")
            
            print(f"\n=== Enemy Team Iteration {i+1} ===")
            print(f"Game State: {game_state}")
            
            if game_state != "running":
                print(f"Game Over! {game_state}")
                break
            
            # Get all enemies
            enemies_response = self.get_enemies()
            enemies = enemies_response.get("enemies", [])
            player = state.get("state", {}).get("player", {})
            player_pos = (player.get("x", 0), player.get("y", 0))
            
            print(f"Number of enemies: {len(enemies)}")
            print(f"Player position: {player_pos}")
            
            # Simple chase: each enemy moves towards player
            for enemy in enemies:
                enemy_idx = enemy["index"]
                enemy_pos = (enemy["x"], enemy["y"])
                enemy_hp = enemy["hp"]
                
                print(f"\n  Enemy {enemy_idx} ({enemy['type']}) at {enemy_pos}, HP: {enemy_hp}")
                
                # Calculate distance to player
                dx = player_pos[0] - enemy_pos[0]
                dy = player_pos[1] - enemy_pos[1]
                distance = math.sqrt(dx*dx + dy*dy)
                
                print(f"    Distance to player: {distance:.1f}")
                
                # Attack if adjacent
                if distance == 1:
                    print(f"    Adjacent! Attacking player at {player_pos}")
                    self.enemy_attack(enemy_idx, player_pos[0], player_pos[1])
                
                # Chase if close enough (Manhattan distance <= 6)
                elif abs(dx) + abs(dy) <= 6:
                    # Move towards player
                    # Simple greedy: move in direction that reduces distance
                    if abs(dx) > abs(dy):
                        direction = "right" if dx > 0 else "left"
                    else:
                        direction = "down" if dy > 0 else "up"
                    
                    print(f"    Moving {direction} (towards player)")
                    result = self.enemy_move(enemy_idx, direction)
                    if result.get("success"):
                        print(f"      ✓ Moved to ({result['x']}, {result['y']})")
                    else:
                        print(f"      ✗ Move failed")
                else:
                    print(f"    Too far away, exploring")
                    # Could add random movement or search pattern
            
            time.sleep(0.5)
    
    def smarter_loop(self, max_iterations: int = 100):
        """More intelligent loop that considers visible cells"""
        
        for i in range(max_iterations):
            state = self.get_state()
            game_state = state.get("state", {}).get("game_state", "running")
            
            print(f"\n=== Smart Enemy Team Iteration {i+1} ===")
            print(f"Game State: {game_state}")
            
            if game_state != "running":
                print(f"Game Over! {game_state}")
                break
            
            # Get all enemies
            enemies_response = self.get_enemies()
            enemies = enemies_response.get("enemies", [])
            
            for enemy in enemies:
                enemy_idx = enemy["index"]
                
                # Get what this enemy can see
                visible = self.get_enemy_visible_cells(enemy_idx, radius=6)
                cells = visible.get("cells", [])
                
                # Look for player in visible cells
                player_visible = None
                for cell in cells:
                    if state.get("state", {}).get("player", {}).get("x") == cell["x"] and \
                       state.get("state", {}).get("player", {}).get("y") == cell["y"]:
                        player_visible = (cell["x"], cell["y"])
                        break
                
                if player_visible:
                    player_x, player_y = player_visible
                    enemy_x, enemy_y = enemy["x"], enemy["y"]
                    
                    # Check if adjacent
                    if abs(player_x - enemy_x) == 1 and abs(player_y - enemy_y) == 0:
                        print(f"Enemy {enemy_idx}: Player spotted adjacent! ATTACK")
                        self.enemy_attack(enemy_idx, player_x, player_y)
                    elif abs(player_x - enemy_x) == 0 and abs(player_y - enemy_y) == 1:
                        print(f"Enemy {enemy_idx}: Player spotted adjacent! ATTACK")
                        self.enemy_attack(enemy_idx, player_x, player_y)
                    else:
                        # Chase
                        dx = player_x - enemy_x
                        dy = player_y - enemy_y
                        
                        if abs(dx) > abs(dy):
                            direction = "right" if dx > 0 else "left"
                        else:
                            direction = "down" if dy > 0 else "up"
                        
                        print(f"Enemy {enemy_idx}: Chasing player, moving {direction}")
                        self.enemy_move(enemy_idx, direction)
                else:
                    print(f"Enemy {enemy_idx}: Player not visible")
            
            time.sleep(0.5)

if __name__ == "__main__":
    agent = EnemyTeamAgent()
    
    # Run simple chase loop
    agent.simple_chase_loop(max_iterations=30)
    
    # Or run smarter loop:
    # agent.smarter_loop(max_iterations=30)
