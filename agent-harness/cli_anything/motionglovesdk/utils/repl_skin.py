"""薄 REPL：可注入 reader / printer，便于单测；命令语法 ``<action> key=value ...``。

CLI 自身也有 REPL（`motionGloveSDK_cli` 无参数启动）。本层 REPL 的价值在于
直接用 harness 的 ``build_argv`` + 信封解析，方便 Agent 对比 argv 与输出。
"""

from __future__ import annotations

import json
from typing import Any, Callable, Dict, List, Optional, Tuple

from . import backend

_HELP = """可用命令:
  status | version | run | export | config.show | config.set | config.clear
  <action> key=value ...    例如: run port=5001 frames=3 timeout_ms=2000
  json on|off               切换输出（默认 on）
  help / exit / quit
"""

_INT_KEYS = {"port", "frames", "timeout_ms", "poll_ms"}


class ReplSkin:
    """极简 REPL 外壳。"""

    def __init__(self, executor: Callable[[str, Dict[str, Any]], Dict[str, Any]],
                 reader: Optional[Callable[[str], Optional[str]]] = None,
                 printer: Optional[Callable[[str], None]] = None,
                 json_mode: bool = True) -> None:
        self._executor = executor
        self._reader = reader or input
        self._print = printer or print
        self.json_mode = json_mode

    def run(self) -> int:
        self._print("MotionGloveSDK harness REPL —— 输入 help 查看命令，exit/quit 退出")
        while True:
            try:
                line = self._reader("mgsdk-harness> ")
            except (EOFError, KeyboardInterrupt):
                break
            if line is None:
                break

            tokens = split_args(line)
            if not tokens:
                continue

            command = tokens[0].lower()
            if command in ("exit", "quit"):
                break
            if command == "help":
                self._print(_HELP.rstrip())
                continue
            if command == "json":
                self._toggle_json(tokens[1:])
                continue

            try:
                action, payload = parse_command(tokens)
                envelope = self._executor(action, payload)
            except backend.CliError as exc:
                self._print("[error] %s" % exc)
                continue
            except backend.CliProtocolError as exc:
                self._print("[protocol-error] %s" % exc)
                continue

            self._print(self._format(envelope))
        return 0

    def _toggle_json(self, args: List[str]) -> None:
        if not args:
            self.json_mode = not self.json_mode
        else:
            self.json_mode = args[0].lower() in ("on", "1", "true", "yes")
        self._print("json output: %s" % ("on" if self.json_mode else "off"))

    def _format(self, envelope: Dict[str, Any]) -> str:
        if self.json_mode:
            return backend.dumps(envelope, compact=True)
        if envelope.get("ok"):
            data = envelope.get("data") or {}
            return "ok  %s" % json.dumps(data, ensure_ascii=False)
        return "error code=%s %s" % (envelope.get("code"), envelope.get("message", ""))


def split_args(line: str) -> List[str]:
    """按空白切分，支持成对单/双引号（路径含空格），并剥离 UTF-8 BOM。"""
    text = line.lstrip("\ufeff")
    tokens: List[str] = []
    current = ""
    in_token = False
    quote = ""

    for char in text:
        if quote:
            if char == quote:
                quote = ""
            else:
                current += char
            continue
        if char in ("'", '"'):
            quote = char
            in_token = True
            continue
        if char.isspace():
            if in_token:
                tokens.append(current)
                current = ""
                in_token = False
            continue
        current += char
        in_token = True
    if in_token:
        tokens.append(current)
    return tokens


def parse_command(tokens: List[str]) -> Tuple[str, Dict[str, Any]]:
    """``["run", "port=5001", "frames=3"]`` -> ``("run", {"port": 5001, "frames": 3})``。"""
    action = tokens[0]
    payload: Dict[str, Any] = {}
    for token in tokens[1:]:
        if "=" not in token:
            raise backend.CliError("expected key=value, got %r" % token)
        key, _, raw = token.partition("=")
        key = key.strip()
        raw = raw.strip()
        if not key:
            raise backend.CliError("empty key in %r" % token)
        if key in _INT_KEYS:
            try:
                payload[key] = int(raw)
            except ValueError:
                raise backend.CliError("%s must be an integer, got %r" % (key, raw))
        else:
            payload[key] = raw
    return action, payload
