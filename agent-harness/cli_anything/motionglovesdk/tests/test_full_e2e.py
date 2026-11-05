"""端到端测试：真调 `motionGloveSDK_cli`，收帧用仓库自带的 Python mock 发送端。

约定：
* 找不到 CLI 二进制时整组 skip（CI 没有构建产物不算失败）；
* 找不到 `tools/mock_glove_sender.py` 时只 skip 依赖发送端的用例；
* 端口随机取空闲 UDP 端口，避免与正在运行的手套软件/示例冲突。
"""

import json
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

from cli_anything.motionglovesdk.utils import backend

REPO_ROOT = Path(__file__).resolve().parents[4]
MOCK_SENDER = REPO_ROOT / "tools" / "mock_glove_sender.py"


def _free_udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@pytest.fixture(scope="session")
def cli_bin():
    path = backend.find_cli_bin()
    if not path:
        pytest.skip("motionGloveSDK_cli 未构建；先构建 motionGloveSDK_cli 目标")
    return path


@pytest.fixture
def sender():
    if not MOCK_SENDER.is_file():
        pytest.skip("tools/mock_glove_sender.py 不存在（部署目录只跑 CLI 用例）")

    processes = []

    def start(port, fmt=42, seconds=10):
        command = [sys.executable, str(MOCK_SENDER), "--port", str(port),
                   "--seconds", str(seconds), "--rate", "60"]
        if fmt != 42:
            command += ["--format", str(fmt)]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        processes.append(process)
        time.sleep(0.8)
        return process

    yield start

    for process in processes:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()


def _call(cli_bin, action, payload=None, timeout=30.0):
    return backend.run_cli(backend.build_argv(action, payload or {}),
                           cli_bin=cli_bin, timeout=timeout)


# --------------------------------------------------------------------------- #
# 只读命令与错误路径
# --------------------------------------------------------------------------- #

def test_status_ok(cli_bin):
    result = _call(cli_bin, "status")
    assert result.ok, result.message
    assert result.exit_code == 0
    assert result.data["sdkVersion"]
    assert result.data["defaults"]["port"] == 5001
    assert result.data["resolved"]["actor"] == "Glove1"


def test_version_ok(cli_bin):
    result = _call(cli_bin, "version")
    status = _call(cli_bin, "status")
    assert result.ok
    assert result.data["cliVersion"] == status.data["cliVersion"]
    assert result.data["sdkVersion"] == status.data["sdkVersion"]


def test_unknown_command_uses_usage_code(cli_bin):
    result = backend.run_cli(["nosuch-command", "--json"], cli_bin=cli_bin)
    assert result.ok is False
    assert result.code == 1
    assert result.exit_code == 1
    assert "unknown command" in result.message


def test_bad_port_is_usage_error(cli_bin):
    # 绕过 harness 的前置校验，验证 CLI 自身的参数错误码
    result = backend.run_cli(["run", "--port", "80", "--json"], cli_bin=cli_bin)
    assert result.ok is False
    assert result.code == 1


def test_run_timeout_returns_code_3(cli_bin):
    port = _free_udp_port()
    result = _call(cli_bin, "run", {"port": port, "frames": 1, "timeout_ms": 250})
    assert result.ok is False
    assert result.code == 3
    assert result.exit_code == 1
    assert result.data["frameCount"] == 0
    assert result.data["layout"] is None


def test_export_missing_directory_is_io_error(cli_bin, tmp_path):
    target = tmp_path / "no-such-dir" / "cap.json"
    result = _call(cli_bin, "export", {"output": str(target), "frames": 1, "timeout_ms": 200})
    assert result.ok is False
    assert result.code == 4
    assert not target.exists()


# --------------------------------------------------------------------------- #
# 真收帧（mock 发送端）
# --------------------------------------------------------------------------- #

def test_run_capture_loopback(cli_bin, sender):
    port = _free_udp_port()
    sender(port)
    result = _call(cli_bin, "run", {"port": port, "frames": 3, "timeout_ms": 8000})
    assert result.ok, result.message
    assert result.data["frameCount"] == 3
    assert result.data["complete"] is True
    assert result.data["layout"] == 42
    frame = result.data["frames"][0]
    assert frame["skeletonCount"] == 42
    assert len(frame["skeletons"]) == 42
    assert frame["remoteIp"] == "127.0.0.1"


def test_run_joint_and_field_filter(cli_bin, sender):
    port = _free_udp_port()
    sender(port)
    result = _call(cli_bin, "run", {
        "port": port,
        "frames": 2,
        "timeout_ms": 8000,
        "joints": ["LeftHand"],
        "fields": "pos,quat",
    })
    assert result.ok, result.message
    skeletons = result.data["frames"][0]["skeletons"]
    assert len(skeletons) == 1
    assert skeletons[0]["name"] == "LeftHand"
    assert set(skeletons[0]) == {"index", "name", "pos", "quat"}


def test_run_32_segment_layout(cli_bin, sender):
    port = _free_udp_port()
    sender(port, fmt=32)
    result = _call(cli_bin, "run", {
        "port": port, "frames": 1, "timeout_ms": 8000, "joints": ["LeftHand"],
    })
    assert result.ok, result.message
    assert result.data["layout"] == 32
    assert result.data["frames"][0]["skeletons"][0]["index"] == 16


def test_export_csv_writes_rows(cli_bin, sender, tmp_path):
    port = _free_udp_port()
    sender(port)
    output = tmp_path / "capture.csv"
    result = _call(cli_bin, "export", {
        "port": port, "frames": 2, "timeout_ms": 8000, "output": str(output),
    })
    assert result.ok, result.message
    assert result.data["format"] == "csv"
    assert output.is_file()
    lines = output.read_text(encoding="utf-8").splitlines()
    assert lines[0].startswith("frame_number,skeleton_count,skeleton_index,skeleton_name")
    assert len(lines) == 1 + 2 * 42
    assert "," in lines[1]


def test_export_json_writes_frames(cli_bin, sender, tmp_path):
    port = _free_udp_port()
    sender(port)
    output = tmp_path / "capture.json"
    result = _call(cli_bin, "export", {
        "port": port, "frames": 2, "timeout_ms": 8000, "output": str(output),
    })
    assert result.ok, result.message
    payload = json.loads(output.read_text(encoding="utf-8"))
    assert payload["frameCount"] == 2
    assert payload["layout"] == 42
    assert len(payload["frames"]) == 2
    assert payload["sdkVersion"]


def test_export_unknown_extension_without_format(cli_bin, tmp_path, sender):
    port = _free_udp_port()
    sender(port)
    result = _call(cli_bin, "export", {
        "port": port, "frames": 1, "timeout_ms": 5000,
        "output": str(tmp_path / "capture.txt"),
    })
    assert result.ok is False
    assert result.code == 1


# --------------------------------------------------------------------------- #
# 会话与 REPL
# --------------------------------------------------------------------------- #

def test_config_roundtrip(cli_bin, tmp_path, monkeypatch):
    session_file = tmp_path / "session.json"
    monkeypatch.setenv("MOTIONGLOVESDK_CLI_SESSION", str(session_file))

    created = _call(cli_bin, "config.set", {"port": 15055, "actor": "Glove9"})
    assert created.ok, created.message
    assert session_file.is_file()

    shown = _call(cli_bin, "config.show")
    assert shown.ok
    assert shown.data["port"] == 15055
    assert shown.data["actor"] == "Glove9"

    status = _call(cli_bin, "status")
    assert status.data["resolved"] == {"port": 15055, "actor": "Glove9"}

    cleared = _call(cli_bin, "config.clear")
    assert cleared.ok
    assert not session_file.exists()
    assert _call(cli_bin, "status").data["resolved"]["port"] == 5001


def test_repl_pipe_with_bom(cli_bin):
    script = "\ufeffstatus --json\njson on\nversion\nexit\n"
    completed = subprocess.run(
        [cli_bin],
        input=script,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert completed.returncode == 0
    assert '"ok":true' in completed.stdout
    assert "motionGloveSDK_cli" in completed.stdout
