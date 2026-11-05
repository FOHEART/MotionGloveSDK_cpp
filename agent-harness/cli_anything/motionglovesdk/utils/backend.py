"""CLI 后端：二进制定位、argv 构造（纯函数）、子进程调用与统一信封解析。

设计要点（与 SKILL.md 对应）：

* ``build_argv`` 是纯函数，单测可完全覆盖映射关系，不需要真实二进制；
* 错误分三层：``CliProtocolError``（stdout 不是合法信封，底层 CLI 版本不匹配）、
  ``CliError``（起不来 / 超时 / 参数非法）、信封原样返回（业务失败默认不抛，
  便于 Agent 读取 ``ok`` / ``code`` 自行决策）；
* 二进制定位顺序：显式参数 > 环境变量 ``MOTIONGLOVESDK_CLI_BIN`` >
  部署目录（与 ``agent-harness`` 同级）> 仓库构建产物 > ``PATH``。
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Union

DEFAULT_BIN_NAME = "motionGloveSDK_cli"
ENV_CLI_BIN = "MOTIONGLOVESDK_CLI_BIN"
DEFAULT_TIMEOUT = 30.0

CapturedValue = Union[str, int, float]


class CliError(RuntimeError):
    """CLI 无法启动、超时或调用参数非法。"""


class CliProtocolError(CliError):
    """stdout 中没有合法的统一信封（通常是 CLI 版本不匹配）。"""


# --------------------------------------------------------------------------- #
# 二进制定位
# --------------------------------------------------------------------------- #

def _binary_names() -> List[str]:
    if os.name == "nt":
        return [DEFAULT_BIN_NAME + ".exe", DEFAULT_BIN_NAME]
    return [DEFAULT_BIN_NAME]


def _deploy_directories() -> List[Path]:
    """部署/源码目录候选：exe 与 agent-harness 同级。"""
    here = Path(__file__).resolve()
    # utils -> motionglovesdk -> cli_anything -> agent-harness -> 部署根
    roots: List[Path] = []
    for index in (3, 4, 2):
        if index < len(here.parents):
            roots.append(here.parents[index])
    return roots


def _build_directories() -> List[Path]:
    """仓库构建产物候选：out/build、out/install* 下任意深度。"""
    here = Path(__file__).resolve()
    if len(here.parents) < 5:
        return []
    repo = here.parents[4]
    candidates: List[Path] = []
    for base_name in ("out/build", "out/install", "out/install_example"):
        base = repo / base_name
        if base.is_dir():
            candidates.append(base)
    return candidates


def find_cli_bin(explicit: Optional[str] = None) -> Optional[str]:
    """按约定顺序定位 ``motionGloveSDK_cli``；找不到返回 ``None``。"""
    names = _binary_names()

    if explicit:
        path = Path(explicit)
        if path.is_file():
            return str(path)
        raise CliError("cli binary not found: %s" % explicit)

    env_value = os.environ.get(ENV_CLI_BIN)
    if env_value:
        path = Path(env_value)
        if path.is_file():
            return str(path)
        raise CliError("%s points to a missing file: %s" % (ENV_CLI_BIN, env_value))

    for directory in _deploy_directories():
        for name in names:
            candidate = directory / name
            if candidate.is_file():
                return str(candidate)

    matches: List[Path] = []
    for base in _build_directories():
        for name in names:
            matches.extend(base.rglob(name))
    if matches:
        matches.sort(key=lambda item: item.stat().st_mtime, reverse=True)
        return str(matches[0])

    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return None


# --------------------------------------------------------------------------- #
# argv 构造（纯函数）
# --------------------------------------------------------------------------- #

_CAPTURE_KEYS = {"port", "actor", "frames", "timeout_ms", "poll_ms", "joints", "fields"}
_EXPORT_KEYS = {"output", "format"}


def _check_keys(payload: Dict[str, Any], allowed: Iterable[str]) -> None:
    unknown = set(payload) - set(allowed)
    if unknown:
        raise CliError("unknown payload key(s): %s" % ", ".join(sorted(unknown)))


def _as_int(name: str, value: Any, minimum: int, maximum: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        raise CliError("%s must be an integer, got %r" % (name, value))
    if parsed < minimum or parsed > maximum:
        raise CliError("%s must be in [%d, %d], got %d" % (name, minimum, maximum, parsed))
    return parsed


def _as_list(value: Any) -> List[str]:
    if isinstance(value, str):
        return [item.strip() for item in value.split(",") if item.strip()]
    if isinstance(value, (list, tuple)):
        return [str(item) for item in value if str(item)]
    raise CliError("expected a string or list, got %r" % (value,))


def _capture_argv(payload: Dict[str, Any]) -> List[str]:
    argv: List[str] = []
    if "port" in payload:
        argv += ["--port", str(_as_int("port", payload["port"], 1024, 65535))]
    if "actor" in payload:
        actor = str(payload["actor"])
        if not actor or len(actor.encode("utf-8")) > 63:
            raise CliError("actor must be 1..63 bytes")
        argv += ["--actor", actor]
    if "frames" in payload:
        argv += ["--frames", str(_as_int("frames", payload["frames"], 0, 1000000))]
    if "timeout_ms" in payload:
        argv += ["--timeout-ms", str(_as_int("timeout_ms", payload["timeout_ms"], 1, 600000))]
    if "poll_ms" in payload:
        argv += ["--poll-ms", str(_as_int("poll_ms", payload["poll_ms"], 1, 1000))]
    if "joints" in payload:
        for joint in _as_list(payload["joints"]):
            argv += ["--joint", joint]
    if "fields" in payload:
        argv += ["--fields", ",".join(_as_list(payload["fields"]))]
    return argv


def build_argv(action: str, payload: Optional[Dict[str, Any]] = None) -> List[str]:
    """把动作名 + 负载映射为 CLI argv（含结尾 ``--json``）。

    支持的动作：``status`` / ``version`` / ``run``（别名 ``listen``、``capture``）/
    ``export`` / ``config.show`` / ``config.set`` / ``config.clear``。
    """
    data: Dict[str, Any] = dict(payload or {})
    name = (action or "").strip()

    if name == "status":
        _check_keys(data, {"port", "actor"})
        argv = ["status"] + _capture_argv({key: data[key] for key in ("port", "actor") if key in data})
    elif name == "version":
        _check_keys(data, set())
        argv = ["version"]
    elif name in ("run", "listen", "capture"):
        _check_keys(data, _CAPTURE_KEYS)
        argv = ["run"] + _capture_argv(data)
    elif name == "export":
        _check_keys(data, _CAPTURE_KEYS | _EXPORT_KEYS)
        output = data.get("output")
        if not output:
            raise CliError("export requires 'output' (path ending with .json or .csv)")
        argv = ["export"] + _capture_argv(data) + ["--output", str(output)]
        if data.get("format"):
            fmt = str(data["format"]).lower()
            if fmt not in ("json", "csv"):
                raise CliError("format must be json or csv, got %r" % fmt)
            argv += ["--format", fmt]
    elif name == "config.show":
        _check_keys(data, {"port", "actor"})
        argv = ["config", "show"] + _capture_argv(data)
    elif name == "config.set":
        _check_keys(data, {"port", "actor"})
        if not data:
            raise CliError("config.set requires 'port' and/or 'actor'")
        argv = ["config", "set"] + _capture_argv(data)
    elif name == "config.clear":
        _check_keys(data, set())
        argv = ["config", "clear"]
    elif name == "help":
        _check_keys(data, set())
        argv = ["help"]
    else:
        raise CliError("unknown action: %s" % action)

    argv.append("--json")
    return argv


# --------------------------------------------------------------------------- #
# 调用与解析
# --------------------------------------------------------------------------- #

class CliResult:
    """一次 CLI 调用的结果：信封 + 进程退出码 + stderr + 实际 argv。"""

    def __init__(self, envelope: Dict[str, Any], exit_code: int,
                 stderr: str, argv: Sequence[str]) -> None:
        self.envelope = envelope
        self.exit_code = int(exit_code)
        self.stderr = stderr or ""
        self.argv = list(argv)

    @property
    def ok(self) -> bool:
        return bool(self.envelope.get("ok"))

    @property
    def code(self) -> int:
        return int(self.envelope.get("code", -1))

    @property
    def message(self) -> str:
        return str(self.envelope.get("message", ""))

    @property
    def data(self) -> Dict[str, Any]:
        value = self.envelope.get("data")
        return value if isinstance(value, dict) else {}

    def __getitem__(self, key: str) -> Any:
        return self.envelope[key]

    def to_dict(self) -> Dict[str, Any]:
        return dict(self.envelope)


def parse_envelope(stdout: str) -> Dict[str, Any]:
    """从 stdout 中取出最后一个合法信封。

    SDK 的诊断日志已被 CLI 改道到 stderr，正常情况下 stdout 只有一行；
    这里反向扫描仍然兼容"主程序内置分流"形态下可能出现的额外输出。
    """
    if not stdout:
        raise CliProtocolError("cli produced no stdout; expected a JSON envelope")

    for line in reversed(stdout.splitlines()):
        text = line.strip().lstrip("\ufeff")
        if not text.startswith("{"):
            continue
        try:
            candidate = json.loads(text)
        except ValueError:
            continue
        if isinstance(candidate, dict) and "ok" in candidate and "code" in candidate:
            candidate.setdefault("message", "")
            candidate.setdefault("data", {})
            return candidate

    raise CliProtocolError(
        "stdout did not contain a CLI envelope (last 400 chars): %r" % stdout[-400:]
    )


def run_cli(argv: Sequence[str], cli_bin: Optional[str] = None,
            timeout: float = DEFAULT_TIMEOUT, cwd: Optional[str] = None) -> CliResult:
    """调用 CLI 并返回 :class:`CliResult`（业务失败不抛异常）。"""
    binary = find_cli_bin(cli_bin)
    if not binary:
        raise CliError(
            "motionGloveSDK_cli not found; set %s or build the 'motionGloveSDK_cli' target"
            % ENV_CLI_BIN
        )

    command = [binary] + list(argv)
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            cwd=cwd,
        )
    except subprocess.TimeoutExpired as exc:
        raise CliError("cli timed out after %.1fs: %s" % (timeout, " ".join(command))) from exc
    except OSError as exc:
        raise CliError("failed to start cli: %s" % exc) from exc

    envelope = parse_envelope(completed.stdout)
    return CliResult(envelope, completed.returncode, completed.stderr, command)


def dumps(envelope: Dict[str, Any], compact: bool = False) -> str:
    """把信封序列化成人类友好 JSON（默认缩进）。"""
    if compact:
        return json.dumps(envelope, ensure_ascii=False, separators=(",", ":"))
    return json.dumps(envelope, ensure_ascii=False, indent=2)
