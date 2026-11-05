"""MotionGloveSDK CLI-Anything harness。

把 `motionGloveSDK_cli` 视为黑盒：本包只负责构造 argv、调用子进程、解析统一 JSON 信封。
"""

__version__ = "0.2.1"

from .utils.backend import (  # noqa: F401
    CliError,
    CliProtocolError,
    CliResult,
    DEFAULT_BIN_NAME,
    ENV_CLI_BIN,
    build_argv,
    find_cli_bin,
    parse_envelope,
    run_cli,
)

__all__ = [
    "CliError",
    "CliProtocolError",
    "CliResult",
    "DEFAULT_BIN_NAME",
    "ENV_CLI_BIN",
    "build_argv",
    "find_cli_bin",
    "parse_envelope",
    "run_cli",
]
