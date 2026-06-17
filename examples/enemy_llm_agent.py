#!/usr/bin/env python3
"""LLM-controlled enemy agent for troll mobs via game-service HTTP API."""

import json
import math
import os
import sys
import time
import urllib.error
import urllib.request


class FakeLLM:
    def decide(self, enemy: dict, player: dict) -> dict:
        dx = player["x"] - enemy["x"]
        dy = player["y"] - enemy["y"]
        distance = math.sqrt(dx * dx + dy * dy)

        if distance == 1:
            return {
                "tool": "enemy_attack",
                "arguments": {
                    "enemy_index": enemy["index"],
                    "target_x": player["x"],
                    "target_y": player["y"],
                },
            }

        if abs(dx) + abs(dy) <= 6:
            direction = "right" if abs(dx) > abs(dy) and dx > 0 else \
                "left" if abs(dx) > abs(dy) else \
                "down" if dy > 0 else "up"
            return {
                "tool": "enemy_move",
                "arguments": {"enemy_index": enemy["index"], "direction": direction},
            }

        return {"tool": "noop", "arguments": {}}


class OllamaLLM:
    def __init__(self, url: str, model: str):
        self.url = url.rstrip("/")
        self.model = model
        self.tokens = 0

    def decide(self, enemy: dict, player: dict) -> dict:
        prompt = {
            "enemy": enemy,
            "player": {"x": player["x"], "y": player["y"], "hp": player["hp"]},
            "allowed_tools": ["enemy_move", "enemy_attack", "noop"],
        }
        body = json.dumps({
            "model": self.model,
            "stream": False,
            "messages": [
                {"role": "system", "content": "Reply ONLY JSON: {\"tool\":\"...\",\"arguments\":{...}}"},
                {"role": "user", "content": json.dumps(prompt)},
            ],
        }).encode()

        req = urllib.request.Request(
            f"{self.url}/api/chat",
            data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                payload = json.loads(resp.read().decode())
                self.tokens += payload.get("eval_count", 0)
                text = payload["message"]["content"]
                start = text.find("{")
                end = text.rfind("}")
                return json.loads(text[start:end + 1])
        except (urllib.error.URLError, KeyError, json.JSONDecodeError):
            return FakeLLM().decide(enemy, player)


class EnemyLLMAgent:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url.rstrip("/")
        provider = os.getenv("LLM_PROVIDER", "mock")
        if provider == "ollama":
            self.llm = OllamaLLM(
                os.getenv("OLLAMA_URL", "http://localhost:11434"),
                os.getenv("OLLAMA_MODEL", "qwen2.5:0.5b"),
            )
        else:
            self.llm = FakeLLM()

    def request(self, method: str, path: str, data: dict | None = None) -> dict:
        body = None if data is None else json.dumps(data).encode()
        req = urllib.request.Request(
            self.base_url + path,
            data=body,
            headers={"Content-Type": "application/json"} if body else {},
            method=method,
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode())

    def run(self, max_steps: int = 50, target_type: str = "troll") -> None:
        self.request("POST", "/api/map", {
            "map_width": 30,
            "map_height": 30,
            "min_node_size": 8,
            "max_depth": 4,
            "seed": int(os.getenv("GAME_SEED", "42")),
        })

        for step in range(max_steps):
            state = self.request("GET", "/api/state")
            if state.get("game_state", "running") != "running":
                print(f"Game finished: {state.get('game_state')}")
                break

            enemies = self.request("GET", "/api/enemies").get("enemies", [])
            trolls = [e for e in enemies if e.get("type") == target_type]
            if not trolls:
                print("No troll enemies left")
                break

            player = state["player"]
            for enemy in trolls:
                decision = self.llm.decide(enemy, player)
                tool = decision.get("tool")
                args = decision.get("arguments", {})
                print(f"step={step} enemy={enemy['index']} tool={tool} args={args}")

                if tool == "enemy_move":
                    self.request("POST", "/api/enemy/move", args)
                elif tool == "enemy_attack":
                    self.request("POST", "/api/enemy/attack", args)

            time.sleep(0.2)


if __name__ == "__main__":
    agent = EnemyLLMAgent(os.getenv("GAME_SERVICE_URL", "http://localhost:8080"))
    agent.run(max_steps=int(os.getenv("MAX_STEPS", "30")))
