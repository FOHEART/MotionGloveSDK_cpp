:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"

echo [fetch_latest] Fetching latest from origin...
git fetch origin
if errorlevel 1 goto :fetch_failed

git pull origin main
if errorlevel 1 goto :pull_failed

echo.
echo [fetch_latest] Done. Current status:
git --no-pager log --oneline -5
exit /b 0

:fetch_failed
echo [ERROR] git fetch failed.
exit /b 1

:pull_failed
echo [ERROR] git pull failed.
exit /b 1

#__SH__
# ---------------------------------------------------------------------------
# 本文件是 cmd + POSIX sh 双解释器脚本，同一份内容在两边的执行方式：
#   Windows : 双击，或 cmd 里运行 scripts\git_pull_latest.bat
#   Linux   : sh scripts/git_pull_latest.bat（或 ./scripts/git_pull_latest.bat）
# 首行对 cmd 是一行标签（整行忽略），对 sh 是 exec：取 #__SH__ 之后的 POSIX 段去 \r
# 重新执行，因此 Windows 检出（CRLF）直接放进 WSL/Linux 也能跑。
# ---------------------------------------------------------------------------
cd "$(dirname "$0")/.." || exit 1

echo "[fetch_latest] Fetching latest from origin..."
git fetch origin || { echo "[ERROR] git fetch failed."; exit 1; }

git pull origin main || { echo "[ERROR] git pull failed."; exit 1; }

echo ""
echo "[fetch_latest] Done. Current status:"
git --no-pager log --oneline -5
