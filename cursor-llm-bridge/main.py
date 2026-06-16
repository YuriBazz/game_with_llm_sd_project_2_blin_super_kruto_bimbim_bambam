"""OpenAI-compatible /v1/chat/completions bridge to Cursor Composer via cursor-sdk."""

import os
import threading
import time
import uuid
from typing import Any

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="cursor-llm-bridge", version="1.0.0")

CURSOR_CWD = os.environ.get("CURSOR_CWD", "/workspace")
DEFAULT_MODEL = os.environ.get("CURSOR_MODEL", "composer-2.5")
MAX_TOKENS = int(os.environ.get("MAX_TOKENS", "5000"))
MAX_TOTAL_TOKENS = int(os.environ.get("MAX_TOTAL_TOKENS", "50000"))
HEALTH_PROBE_INTERVAL_SEC = int(os.environ.get("HEALTH_PROBE_INTERVAL_SEC", "90"))
LLM_PROMPT_PATH = os.environ.get("LLM_PROMPT_PATH", "/app/llm_prompt.txt")

_token_lock = threading.Lock()
_total_tokens_used = 0
_last_health_probe_at = 0.0
_llm_prompt_cache: str | None = None


class ChatMessage(BaseModel):
    role: str
    content: str


class ChatCompletionsRequest(BaseModel):
    model: str = DEFAULT_MODEL
    messages: list[ChatMessage] = Field(default_factory=list)
    temperature: float | None = None
    max_tokens: int | None = None


def _estimate_tokens(text: str) -> int:
    if not text:
        return 0
    return max(1, len(text) // 4)


def _load_llm_prompt() -> str:
    global _llm_prompt_cache
    if _llm_prompt_cache is not None:
        return _llm_prompt_cache
    try:
        with open(LLM_PROMPT_PATH, encoding="utf-8") as handle:
            _llm_prompt_cache = handle.read().strip()
    except OSError:
        _llm_prompt_cache = ""
    return _llm_prompt_cache


def _token_budget_status() -> dict[str, int]:
    with _token_lock:
        used = _total_tokens_used
    return {
        "tokens_used": used,
        "max_total_tokens": MAX_TOTAL_TOKENS,
        "max_tokens": MAX_TOKENS,
    }


def _ensure_token_budget(additional_tokens: int) -> None:
    with _token_lock:
        if _total_tokens_used + additional_tokens > MAX_TOTAL_TOKENS:
            raise HTTPException(
                status_code=429,
                detail=(
                    f"Token budget exceeded ({_total_tokens_used}/{MAX_TOTAL_TOKENS}). "
                    "LLM agents stopped."
                ),
            )


def _record_token_usage(prompt_tokens: int, completion_tokens: int) -> dict[str, int]:
    total = prompt_tokens + completion_tokens
    with _token_lock:
        global _total_tokens_used
        _total_tokens_used += total
        used = _total_tokens_used
    return {
        "prompt_tokens": prompt_tokens,
        "completion_tokens": completion_tokens,
        "total_tokens": total,
        "session_total_tokens": used,
    }


def _format_prompt(messages: list[ChatMessage]) -> str:
    parts: list[str] = []
    for msg in messages:
        role = msg.role.strip().lower()
        content = msg.content.strip()
        if not content:
            continue
        if role == "system":
            parts.append(f"SYSTEM:\n{content}")
        elif role == "assistant":
            parts.append(f"ASSISTANT:\n{content}")
        else:
            parts.append(f"USER:\n{content}")
    if not parts:
        raise ValueError("messages must not be empty")
    return "\n\n".join(parts)


def _extract_text(result: Any) -> str:
    if result is None:
        return ""
    if isinstance(result, str):
        return result.strip()
    text = getattr(result, "result", None)
    if isinstance(text, str) and text.strip():
        return text.strip()
    if text is not None:
        return str(text).strip()
    if hasattr(result, "text"):
        t = result.text()
        if isinstance(t, str):
            return t.strip()
    return str(result).strip()


def _run_cursor_prompt(full_prompt: str, model: str) -> str:
    api_key = os.environ.get("CURSOR_API_KEY", "").strip()
    if not api_key:
        raise HTTPException(status_code=503, detail="CURSOR_API_KEY is not set")

    try:
        from cursor_sdk import Agent, AgentOptions, LocalAgentOptions
    except ImportError as exc:
        raise HTTPException(status_code=500, detail="cursor-sdk not installed") from exc

    try:
        result = Agent.prompt(
            full_prompt,
            AgentOptions(
                api_key=api_key,
                model=model or DEFAULT_MODEL,
                local=LocalAgentOptions(cwd=CURSOR_CWD),
                mode="plan",
            ),
        )
    except Exception as exc:
        raise HTTPException(status_code=502, detail=f"Cursor SDK error: {exc}") from exc

    status = getattr(result, "status", "finished")
    if status == "error":
        raise HTTPException(status_code=502, detail="Cursor run failed")

    content = _extract_text(result)
    if not content:
        raise HTTPException(status_code=502, detail="Empty response from Cursor")
    return content


def _maybe_run_health_probe() -> dict[str, Any]:
    global _last_health_probe_at

    now = time.time()
    with _token_lock:
        if now - _last_health_probe_at < HEALTH_PROBE_INTERVAL_SEC:
            return {"probe": "skipped", "next_probe_in_sec": int(
                HEALTH_PROBE_INTERVAL_SEC - (now - _last_health_probe_at)
            )}
        _last_health_probe_at = now

    system_prompt = _load_llm_prompt()
    if not system_prompt:
        return {"probe": "skipped", "reason": "llm_prompt_missing"}

    user_prompt = (
        "Confirm readiness. Reply with JSON only, one line, no state echo. "
        'Example: {"tool":"move","arguments":{"direction":"up"}}'
    )
    full_prompt = (
        "You are a game AI controller. Reply with ONLY the JSON object requested "
        "in the user message. Do not edit files, run shell commands, or echo game state.\n\n"
        f"SYSTEM:\n{system_prompt}\n\nUSER:\n{user_prompt}"
    )

    estimated = _estimate_tokens(full_prompt) + min(MAX_TOKENS, 64)
    _ensure_token_budget(estimated)

    content = _run_cursor_prompt(full_prompt, DEFAULT_MODEL)
    usage = _record_token_usage(_estimate_tokens(full_prompt), _estimate_tokens(content))
    return {"probe": "ok", "usage": usage}


@app.get("/health")
def health() -> dict[str, Any]:
    key = os.environ.get("CURSOR_API_KEY", "")
    budget = _token_budget_status()
    if not key:
        return {"status": "missing_api_key", **budget}

    if budget["tokens_used"] >= MAX_TOTAL_TOKENS:
        return {
            "status": "token_budget_exceeded",
            "model_default": DEFAULT_MODEL,
            **budget,
        }

    try:
        probe = _maybe_run_health_probe()
    except HTTPException as exc:
        if exc.status_code == 429:
            return {
                "status": "token_budget_exceeded",
                "model_default": DEFAULT_MODEL,
                **budget,
                "detail": exc.detail,
            }
        return {
            "status": "degraded",
            "model_default": DEFAULT_MODEL,
            **budget,
            "health_probe": {"probe": "failed", "detail": exc.detail},
        }

    status = "ok" if probe.get("probe") in {"ok", "skipped"} else "degraded"
    return {
        "status": status,
        "model_default": DEFAULT_MODEL,
        **budget,
        "health_probe": probe,
    }


@app.get("/v1/models")
def list_models() -> dict[str, Any]:
    return {
        "object": "list",
        "data": [
            {"id": "composer-2.5", "object": "model"},
        ],
    }


@app.post("/v1/chat/completions")
def chat_completions(body: ChatCompletionsRequest) -> dict[str, Any]:
    try:
        prompt = _format_prompt(body.messages)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    completion_cap = body.max_tokens if body.max_tokens is not None else MAX_TOKENS
    completion_cap = max(1, min(completion_cap, MAX_TOKENS))

    # Game robot: JSON-only replies, no repo edits.
    full_prompt = (
        "You are a game AI controller. Reply with ONLY the JSON object requested "
        "in the user message. Do not edit files, run shell commands, or echo game state.\n\n"
        + prompt
    )

    estimated_prompt_tokens = _estimate_tokens(full_prompt)
    _ensure_token_budget(estimated_prompt_tokens + completion_cap)

    content = _run_cursor_prompt(full_prompt, body.model or DEFAULT_MODEL)
    usage = _record_token_usage(estimated_prompt_tokens, _estimate_tokens(content))

    model = body.model or DEFAULT_MODEL
    return {
        "id": f"chatcmpl-{uuid.uuid4().hex}",
        "object": "chat.completion",
        "created": int(time.time()),
        "model": model,
        "choices": [
            {
                "index": 0,
                "message": {"role": "assistant", "content": content},
                "finish_reason": "stop",
            }
        ],
        "usage": usage,
    }
