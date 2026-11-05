:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "ROOT_DIR=%%~fI"

where code >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 未找到 code 命令。安装 VSCode 时请勾选 "Add to PATH"，
    echo         或把 "...\Microsoft VS Code\bin" 加入 PATH 后重试。
    exit /b 1
)

code "%ROOT_DIR%"
exit /b %errorlevel%

#__SH__
# ---------------------------------------------------------------------------
# 本文件是 cmd + POSIX sh 双解释器脚本，同一份内容在两边的执行方式：
#   Windows : 双击根目录的 open_in_vscode.bat，或 cmd 里直接运行
#   Linux   : sh open_in_vscode.bat（或 ./open_in_vscode.bat）
# 首行对 cmd 是一行标签（整行忽略），对 sh 是 exec：取 #__SH__ 之后的 POSIX 段去 \r
# 重新执行，因此 Windows 检出（CRLF）直接放进 WSL/Linux 也能跑。
# ---------------------------------------------------------------------------
cd "$(dirname "$0")" || exit 1

if ! command -v code >/dev/null 2>&1; then
    echo "[ERROR] 未找到 code 命令。请先安装 VSCode 并在命令面板执行" >&2
    echo '        "Shell Command: Install '\''code'\'' command in PATH"。' >&2
    exit 1
fi

code "$PWD"
