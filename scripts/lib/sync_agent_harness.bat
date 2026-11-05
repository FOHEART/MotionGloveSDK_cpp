::; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  sync_agent_harness.bat -- 把 agent-harness + 根 SKILL.md 同步到部署目录
REM
REM  用法（Windows）：
REM    scripts\lib\sync_agent_harness.bat out\install_example\x64-Debug
REM
REM  用法（Linux）：
REM    sh scripts/lib/sync_agent_harness.bat out/install_example/x64-Debug
REM
REM  约定：
REM    - harness 与 exe 同级部署，靠相对位置自动定位 CLI，用户无需配置；
REM    - SKILL.md 只保留根目录一份，这里复制过去，不在 harness 内放副本；
REM    - 排除 __pycache__ / .pytest_cache / *.egg-info，避免把缓存打进分发包。
REM    - robocopy 退出码小于 8 都算成功（1=有复制，2=有多余文件）。
REM ============================================================================
setlocal EnableDelayedExpansion
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
cd /d "%ROOT%"

set "TARGET=%~1"
if "%TARGET%"=="" (
    echo [ERROR] 用法: scripts\lib\sync_agent_harness.bat ^<目标目录^>
    exit /b 1
)

if not exist "%TARGET%" md "%TARGET%" 2>nul

if not exist "%ROOT%\agent-harness" (
    echo [ERROR] 未找到 agent-harness 目录
    exit /b 1
)

robocopy "%ROOT%\agent-harness" "%TARGET%\agent-harness" /E /XD __pycache__ .pytest_cache *.egg-info /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo [ERROR] robocopy agent-harness 失败，退出码 !errorlevel!
    exit /b 1
)

if not exist "%ROOT%\SKILL.md" (
    echo [ERROR] 未找到 SKILL.md
    exit /b 1
)
copy /y "%ROOT%\SKILL.md" "%TARGET%\SKILL.md" >nul
if errorlevel 1 (
    echo [ERROR] 复制 SKILL.md 失败
    exit /b 1
)

echo     agent harness: %TARGET%\agent-harness + SKILL.md
exit /b 0

#__SH__
# ---------------------------------------------------------------------------
# cmd + POSIX sh 双解释器脚本：
#   Windows : cmd 里运行 scripts\lib\sync_agent_harness.bat <目标目录>
#   Linux   : sh scripts/lib/sync_agent_harness.bat <目标目录>
# 用 tar 复制并排除缓存（不依赖 rsync）。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd) || exit 1
cd "$ROOT" || exit 1

TARGET=${1:-}
if [ -z "$TARGET" ]; then
    echo "[ERROR] 用法: sh scripts/lib/sync_agent_harness.bat <目标目录>"
    exit 1
fi

if [ ! -d "$ROOT/agent-harness" ]; then
    echo "[ERROR] 未找到 agent-harness 目录"
    exit 1
fi
if [ ! -f "$ROOT/SKILL.md" ]; then
    echo "[ERROR] 未找到 SKILL.md"
    exit 1
fi

mkdir -p "$TARGET" || exit 1
rm -rf "$TARGET/agent-harness"
tar -cf - --exclude='__pycache__' --exclude='.pytest_cache' --exclude='*.egg-info' agent-harness \
    | ( cd "$TARGET" && tar -xf - ) || exit 1
cp "$ROOT/SKILL.md" "$TARGET/SKILL.md" || exit 1

echo "    agent harness: $TARGET/agent-harness + SKILL.md"
