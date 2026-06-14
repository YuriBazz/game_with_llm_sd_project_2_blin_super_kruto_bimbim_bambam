#!/usr/bin/env python3
"""
Example: Player Agent using Real-Time Game API

This demonstrates how a Player LLM Agent can interact with the game-service
"""

import requests
import json
import time
from typing import Optional

class PlayerAgent:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
    
    def get_state(self) -> dict:
        """Get full game state"""
        resp = requests.get(f"{self.base_url}/api/state")
        return resp.json()
    
    def get_visible_cells(self, radius: int = 6) -> dict:
        """Get cells visible to the player"""
        resp = requests.get(f"{self.base_url}/api/visible_cells?radius={radius}")
        return resp.json()
    
    def get_available_actions(self) -> list:
        """Get available actions for the player"""
        resp = requests.get(f"{self.base_url}/api/available_actions")
        return resp.json().get("actions", [])
    
    def move(self, direction: str) -> dict:
        """Move player in direction: up|down|left|right"""
        resp = requests.post(
            f"{self.base_url}/api/move",
            json={"direction": direction}
        )
        return resp.json()
    
    def attack(self, target_x: int, target_y: int) -> dict:
        """Attack enemy at coordinates"""
        resp = requests.post(
            f"{self.base_url}/api/attack",
            json={"target_x": target_x, "target_y": target_y}
        )
        return resp.json()
    
    def pickup_item(self) -> dict:
        """Pick up items on current tile"""
        resp = requests.post(f"{self.base_url}/api/pickup_item")
        return resp.json()
    
    def use_item(self, item_id: str) -> dict:
        """Use item from inventory"""
        resp = requests.post(
            f"{self.base_url}/api/use_item",
            json={"item_id": item_id}
        )
        return resp.json()
    
    def simple_loop(self, max_iterations: int = 100):
        """Simple random walk loop"""
        import random
        
        for i in range(max_iterations):
            state = self.get_state()
            game_state = state.get("state", {}).get("game_state", "running")
            
            print(f"\n=== Iteration {i+1} ===")
            print(f"Game State: {game_state}")
            print(f"Player HP: {state.get('state', {}).get('player', {}).get('hp', '?')}")
            print(f"Enemy Count: {len(state.get('state', {}).get('enemies', []))}")
            
            if game_state != "running":
                print(f"Game Over! {game_state}")
                break
            
            # Get available actions
            actions = self.get_available_actions()
            print(f"Available Actions: {actions}")
            
            # Simple random action
            if "move" in actions:
                direction = random.choice(["up", "down", "left", "right"])
                print(f"Moving {direction}")
                self.move(direction)
            elif "attack" in actions:
                # Find an enemy to attack
                enemies = state.get("state", {}).get("enemies", [])
                if enemies:
                    enemy = enemies[0]
                    print(f"Attacking enemy at ({enemy['x']}, {enemy['y']})")
                    self.attack(enemy["x"], enemy["y"])
            
            time.sleep(0.5)

if __name__ == "__main__":
    agent = PlayerAgent()
    
    # Start a new game first!
    print("Starting new game...")
    resp = requests.post(
        "http://localhost:8080/api/map",
        json={
            "seed": 42,
            "room_count": 5,
            "room_max_size": 12,
            "room_min_size": 6
        }
    )
    print(f"Game started: {resp.json().get('success')}")
    
    # Run simple loop
    agent.simple_loop(max_iterations=20)
