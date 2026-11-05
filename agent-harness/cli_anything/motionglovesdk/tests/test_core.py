"""全 mock 单测：argv 映射、信封解析、二进制定位、REPL 皮肤、错误路径。

这些用例不启动真实 CLI，因此可以在没有构建产物的机器/CI 上运行。
"""

import json
import os
import stat
from types import SimpleNamespace

import pytest

from cli_anything.motionglovesdk.utils import backend
from cli_anything.motionglovesdk.utils import repl_skin

VALID_ENVELOPE = '{"ok":true,"code":0,"message":"","data":{"frameCount":3}}'


# --------------------------------------------------------------------------- #
# build_argv：纯函数映射
# --------------------------------------------------------------------------- #

def test_build_argv_status_defaults_to_json():
    assert backend.build_argv("status") == ["status", "--json"]


def test_build_argv_status_port_and_actor():
    assert backend.build_argv("status", {"port": 5002, "actor": "Glove1"}) == [
        "status", "--port", "5002", "--actor", "Glove1", "--json",
    ]


def test_build_argv_run_full_mapping():
    argv = backend.build_argv("run", {
        "port": 5001,
        "actor": "Glove1",
        "frames": 3,
        "timeout_ms": 2000,
        "poll_ms": 20,
        "joints": ["LeftHand", "RightHandIndex1"],
        "fields": "pos,quat",
    })
    assert argv == [
        "run",
        "--port", "5001",
        "--actor", "Glove1",
        "--frames", "3",
        "--timeout-ms", "2000",
        "--poll-ms", "20",
        "--joint", "LeftHand",
        "--joint", "RightHandIndex1",
        "--fields", "pos,quat",
        "--json",
    ]


def test_build_argv_run_alias_listen():
    assert backend.build_argv("listen", {"frames": 1})[0] == "run"


def test_build_argv_export_includes_output_and_format():
    argv = backend.build_argv("export", {
        "output": "out/cap.csv",
        "format": "csv",
        "frames": 2,
    })
    assert argv[0] == "export"
    assert argv[-1] == "--json"
    assert argv[argv.index("--output") + 1] == "out/cap.csv"
    assert argv[argv.index("--format") + 1] == "csv"


def test_build_argv_export_requires_output():
    with pytest.raises(backend.CliError):
        backend.build_argv("export", {"frames": 1})


def test_build_argv_export_rejects_bad_format():
    with pytest.raises(backend.CliError):
        backend.build_argv("export", {"output": "a.csv", "format": "xml"})


def test_build_argv_rejects_unknown_payload_key():
    with pytest.raises(backend.CliError):
        backend.build_argv("status", {"ports": 5001})


def test_build_argv_rejects_out_of_range_port():
    with pytest.raises(backend.CliError):
        backend.build_argv("run", {"port": 80})


def test_build_argv_rejects_unknown_action():
    with pytest.raises(backend.CliError):
        backend.build_argv("frobnicate")


def test_build_argv_config_set_requires_value():
    with pytest.raises(backend.CliError):
        backend.build_argv("config.set", {})


def test_build_argv_config_clear():
    assert backend.build_argv("config.clear") == ["config", "clear", "--json"]


def test_build_argv_joints_accepts_comma_string():
    argv = backend.build_argv("run", {"joints": "LeftHand,RightHand"})
    assert argv.count("--joint") == 2


# --------------------------------------------------------------------------- #
# 信封解析
# --------------------------------------------------------------------------- #

def test_parse_envelope_picks_last_valid_line():
    stdout = 'debug line\r\n{"ok":false,"code":2,"message":"x","data":{}}\r\n' + VALID_ENVELOPE + "\r\n"
    envelope = backend.parse_envelope(stdout)
    assert envelope["ok"] is True
    assert envelope["data"]["frameCount"] == 3


def test_parse_envelope_strips_bom():
    envelope = backend.parse_envelope("\ufeff" + VALID_ENVELOPE)
    assert envelope["code"] == 0


def test_parse_envelope_fills_missing_fields():
    envelope = backend.parse_envelope('{"ok":true,"code":0}')
    assert envelope["message"] == ""
    assert envelope["data"] == {}


def test_parse_envelope_rejects_human_text():
    with pytest.raises(backend.CliProtocolError):
        backend.parse_envelope("frames=3/3 elapsed=35ms")


def test_parse_envelope_rejects_empty_output():
    with pytest.raises(backend.CliProtocolError):
        backend.parse_envelope("")


# --------------------------------------------------------------------------- #
# 二进制定位
# --------------------------------------------------------------------------- #

def _fake_binary(path):
    path.write_text("", encoding="utf-8")
    if os.name != "nt":
        path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


def test_find_cli_bin_from_env(tmp_path, monkeypatch):
    fake = _fake_binary(tmp_path / backend.DEFAULT_BIN_NAME)
    monkeypatch.setenv(backend.ENV_CLI_BIN, str(fake))
    assert backend.find_cli_bin() == str(fake)


def test_find_cli_bin_env_missing_file(tmp_path, monkeypatch):
    monkeypatch.setenv(backend.ENV_CLI_BIN, str(tmp_path / "missing.exe"))
    with pytest.raises(backend.CliError):
        backend.find_cli_bin()


def test_find_cli_bin_explicit_missing(tmp_path):
    with pytest.raises(backend.CliError):
        backend.find_cli_bin(str(tmp_path / "missing.exe"))


# --------------------------------------------------------------------------- #
# run_cli（subprocess 全 mock）
# --------------------------------------------------------------------------- #

def test_run_cli_parses_envelope_and_exit_code(tmp_path, monkeypatch):
    fake = _fake_binary(tmp_path / backend.DEFAULT_BIN_NAME)
    monkeypatch.setenv(backend.ENV_CLI_BIN, str(fake))
    captured = {}

    def fake_run(command, **kwargs):
        captured["command"] = command
        captured["kwargs"] = kwargs
        return SimpleNamespace(returncode=0, stdout=VALID_ENVELOPE + "\n", stderr="sdk log")

    monkeypatch.setattr(backend.subprocess, "run", fake_run)

    result = backend.run_cli(["status", "--json"])
    assert result.ok is True
    assert result.exit_code == 0
    assert result.data["frameCount"] == 3
    assert result.stderr == "sdk log"
    assert captured["command"][0] == str(fake)
    assert captured["kwargs"]["encoding"] == "utf-8"


def test_run_cli_timeout_is_cli_error(tmp_path, monkeypatch):
    fake = _fake_binary(tmp_path / backend.DEFAULT_BIN_NAME)
    monkeypatch.setenv(backend.ENV_CLI_BIN, str(fake))

    def fake_run(command, **kwargs):
        raise backend.subprocess.TimeoutExpired(cmd=command, timeout=1)

    monkeypatch.setattr(backend.subprocess, "run", fake_run)
    with pytest.raises(backend.CliError):
        backend.run_cli(["status", "--json"], timeout=1)


def test_run_cli_business_failure_does_not_raise(tmp_path, monkeypatch):
    fake = _fake_binary(tmp_path / backend.DEFAULT_BIN_NAME)
    monkeypatch.setenv(backend.ENV_CLI_BIN, str(fake))
    envelope = '{"ok":false,"code":3,"message":"no frames","data":{"frameCount":0}}'

    monkeypatch.setattr(
        backend.subprocess, "run",
        lambda command, **kwargs: SimpleNamespace(returncode=1, stdout=envelope, stderr=""),
    )
    result = backend.run_cli(["run", "--json"])
    assert result.ok is False
    assert result.code == 3
    assert result.exit_code == 1
    assert result.data["frameCount"] == 0


# --------------------------------------------------------------------------- #
# REPL 皮肤
# --------------------------------------------------------------------------- #

def test_split_args_handles_quotes_and_bom():
    tokens = repl_skin.split_args('\ufeffexport output="C:\\tmp dir\\cap.csv" frames=2')
    assert tokens[0] == "export"
    assert tokens[1] == "output=C:\\tmp dir\\cap.csv"


def test_parse_command_converts_integer_keys():
    action, payload = repl_skin.parse_command(["run", "port=5001", "frames=3", "joints=LeftHand"])
    assert action == "run"
    assert payload == {"port": 5001, "frames": 3, "joints": "LeftHand"}


def test_parse_command_rejects_bare_token():
    with pytest.raises(backend.CliError):
        repl_skin.parse_command(["run", "5001"])


def test_repl_skin_dispatch_and_json_toggle():
    lines = iter(["status", "json off", "version", "exit"])
    printed = []
    calls = []

    def executor(action, payload):
        calls.append((action, payload))
        return {"ok": True, "code": 0, "message": "", "data": {"action": action}}

    skin = repl_skin.ReplSkin(executor, reader=lambda prompt: next(lines, None),
                              printer=printed.append)
    assert skin.run() == 0
    assert [call[0] for call in calls] == ["status", "version"]
    assert any("json output: off" in line for line in printed)
    assert skin.json_mode is False


def test_repl_skin_reports_executor_error():
    lines = iter(["run port=80", "exit"])
    printed = []

    def executor(action, payload):
        raise backend.CliError("port out of range")

    skin = repl_skin.ReplSkin(executor, reader=lambda prompt: next(lines, None),
                              printer=printed.append)
    skin.run()
    assert any("[error] port out of range" in line for line in printed)


def test_dumps_compact_is_single_line():
    text = backend.dumps({"ok": True, "code": 0, "message": "", "data": {}}, compact=True)
    assert "\n" not in text
    assert json.loads(text)["ok"] is True
