"""Click 入口：把 ``motionGloveSDK_cli`` 的子命令映射成 Python 命令。

所有子命令默认输出统一 JSON 信封（stdout 一行），失败时进程退出码为 1，
细分错误码在信封 ``code`` 里，便于 Agent 直接判定。
"""

from __future__ import annotations

import json
from typing import Any, Dict, List, Optional

import click

from . import __version__
from .utils import backend
from .utils.repl_skin import ReplSkin

CONTEXT_SETTINGS = {"help_option_names": ["-h", "--help"]}


def _settings(ctx: click.Context) -> Dict[str, Any]:
    return ctx.ensure_object(dict)


def _execute(ctx: click.Context, action: str, payload: Dict[str, Any]) -> backend.CliResult:
    settings = _settings(ctx)
    argv = backend.build_argv(action, payload)
    return backend.run_cli(
        argv,
        cli_bin=settings.get("cli_bin"),
        timeout=settings.get("timeout", backend.DEFAULT_TIMEOUT),
    )


def _finish(ctx: click.Context, result: backend.CliResult) -> None:
    settings = _settings(ctx)
    click.echo(backend.dumps(result.envelope, compact=settings.get("compact", True)))
    if result.stderr.strip():
        click.echo(result.stderr.rstrip(), err=True)
    if not result.ok:
        raise SystemExit(1)


def _failure(message: str) -> None:
    """harness 层错误的码位固定为 -1：表示没拿到 CLI 信封就失败了。"""
    click.echo(json.dumps(
        {"ok": False, "code": -1, "message": message, "data": {}},
        ensure_ascii=False,
    ))
    raise SystemExit(1)


@click.group(context_settings=CONTEXT_SETTINGS)
@click.option("--cli-bin", default=None, envvar=backend.ENV_CLI_BIN,
              help="motionGloveSDK_cli 路径（默认自动定位）")
@click.option("--timeout", type=float, default=backend.DEFAULT_TIMEOUT, show_default=True,
              help="单次 CLI 调用的墙钟超时（秒）")
@click.option("--compact/--pretty", default=True, show_default=True,
              help="信封单行输出 / 缩进输出")
@click.version_option(version=__version__, prog_name="motionglovesdk-cli")
@click.pass_context
def main(ctx: click.Context, cli_bin: Optional[str], timeout: float, compact: bool) -> None:
    """MotionGlove 手套 SDK 的无头命令行 harness。"""
    ctx.obj = {"cli_bin": cli_bin, "timeout": timeout, "compact": compact}


@main.command()
@click.option("--port", type=int, default=None, help="预览使用的端口")
@click.option("--actor", default=None, help="预览使用的数据流名前缀")
@click.pass_context
def status(ctx: click.Context, port: Optional[int], actor: Optional[str]) -> None:
    """SDK 版本、默认参数与会话状态。"""
    payload: Dict[str, Any] = {}
    if port is not None:
        payload["port"] = port
    if actor:
        payload["actor"] = actor
    try:
        result = _execute(ctx, "status", payload)
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@main.command()
@click.pass_context
def version(ctx: click.Context) -> None:
    """CLI 与 SDK 版本。"""
    try:
        result = _execute(ctx, "version", {})
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


def _capture_payload(port: Optional[int], actor: Optional[str], frames: int,
                     timeout_ms: int, poll_ms: int, joints: List[str],
                     fields: str) -> Dict[str, Any]:
    payload: Dict[str, Any] = {"frames": frames, "timeout_ms": timeout_ms, "poll_ms": poll_ms}
    if port is not None:
        payload["port"] = port
    if actor:
        payload["actor"] = actor
    if joints:
        payload["joints"] = list(joints)
    if fields:
        payload["fields"] = fields
    return payload


@main.command()
@click.option("--port", type=int, default=None, help="UDP 监听端口（默认 5001）")
@click.option("--actor", default=None, help="数据流名前缀（默认 Glove1）")
@click.option("--frames", type=int, default=1, show_default=True, help="采集帧数，0 表示直到超时")
@click.option("--timeout-ms", type=int, default=3000, show_default=True, help="采集超时（毫秒）")
@click.option("--poll-ms", type=int, default=10, show_default=True, help="轮询间隔（毫秒）")
@click.option("--joint", "joints", multiple=True, help="只输出指定骨骼（名称或序号），可重复")
@click.option("--fields", default="pos,euler,quat", show_default=True,
              help="输出字段：pos,euler,quat 的子集")
@click.pass_context
def run(ctx: click.Context, port: Optional[int], actor: Optional[str], frames: int,
        timeout_ms: int, poll_ms: int, joints: List[str], fields: str) -> None:
    """监听 UDP 并采集手套帧。"""
    payload = _capture_payload(port, actor, frames, timeout_ms, poll_ms, joints, fields)
    try:
        result = _execute(ctx, "run", payload)
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@main.command()
@click.option("--output", required=True, type=click.Path(dir_okay=False),
              help="输出文件（.json / .csv；目录必须存在）")
@click.option("--format", "fmt", type=click.Choice(["json", "csv"]), default=None,
              help="显式指定格式（默认按扩展名推断）")
@click.option("--port", type=int, default=None, help="UDP 监听端口（默认 5001）")
@click.option("--actor", default=None, help="数据流名前缀（默认 Glove1）")
@click.option("--frames", type=int, default=1, show_default=True, help="采集帧数，0 表示直到超时")
@click.option("--timeout-ms", type=int, default=3000, show_default=True, help="采集超时（毫秒）")
@click.option("--poll-ms", type=int, default=10, show_default=True, help="轮询间隔（毫秒）")
@click.option("--joint", "joints", multiple=True, help="只导出指定骨骼（名称或序号），可重复")
@click.option("--fields", default="pos,euler,quat", show_default=True,
              help="输出字段：pos,euler,quat 的子集")
@click.pass_context
def export(ctx: click.Context, output: str, fmt: Optional[str], port: Optional[int],
           actor: Optional[str], frames: int, timeout_ms: int, poll_ms: int,
           joints: List[str], fields: str) -> None:
    """采集若干帧并写成 JSON / CSV 文件。"""
    payload = _capture_payload(port, actor, frames, timeout_ms, poll_ms, joints, fields)
    payload["output"] = output
    if fmt:
        payload["format"] = fmt
    try:
        result = _execute(ctx, "export", payload)
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@main.group()
def config() -> None:
    """会话默认值（端口、数据流名）的查看与修改。"""


@config.command("show")
@click.pass_context
def config_show(ctx: click.Context) -> None:
    """查看当前会话。"""
    try:
        result = _execute(ctx, "config.show", {})
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@config.command("set")
@click.option("--port", type=int, default=None)
@click.option("--actor", default=None)
@click.pass_context
def config_set(ctx: click.Context, port: Optional[int], actor: Optional[str]) -> None:
    """写入会话默认值（至少给一项）。"""
    payload: Dict[str, Any] = {}
    if port is not None:
        payload["port"] = port
    if actor:
        payload["actor"] = actor
    if not payload:
        _failure("config set requires --port and/or --actor")
        return
    try:
        result = _execute(ctx, "config.set", payload)
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@config.command("clear")
@click.pass_context
def config_clear(ctx: click.Context) -> None:
    """删除会话文件，恢复内置默认值。"""
    try:
        result = _execute(ctx, "config.clear", {})
    except backend.CliError as exc:
        _failure(str(exc))
        return
    _finish(ctx, result)


@main.command()
@click.pass_context
def repl(ctx: click.Context) -> None:
    """进入 harness REPL（命令语法：action key=value ...）。"""
    settings = _settings(ctx)

    def executor(action: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        argv = backend.build_argv(action, payload)
        return backend.run_cli(
            argv,
            cli_bin=settings.get("cli_bin"),
            timeout=settings.get("timeout", backend.DEFAULT_TIMEOUT),
        ).envelope

    raise SystemExit(ReplSkin(executor).run())


if __name__ == "__main__":
    main()
