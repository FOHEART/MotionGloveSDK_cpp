:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  chcp 必须在任何中文（多字节）内容之前执行：cmd 切换代码页会重定位批处理读取位置，
REM  若前面已有多字节内容，切换后可能错位跳行（本地与 CI 初始代码页不同，症状不稳定）。
REM  package.bat -- 打源码分发包（含可选 preBuild 预编译产物）
REM
REM  用法（Windows）：
REM    scripts\package.bat              版本号取自版本头文件
REM    scripts\package.bat 0.2.2        指定版本号
REM
REM  用法（Linux）：
REM    sh scripts/package.bat [版本号]   需要 PowerShell 7（pwsh）
REM
REM  产物：out\pack\MotionGloveSDK_cpp_v<版本号>.zip
REM  逻辑在 tools\package_source.ps1，暂存目录也在 out\pack 下。
REM ============================================================================
setlocal
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "OUTDIR=%ROOT%\out\pack"
if not exist "%OUTDIR%" md "%OUTDIR%" 2>nul

if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\package_source.ps1" -OutputDir "%OUTDIR%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\package_source.ps1" -OutputDir "%OUTDIR%" -Version "%~1"
)
exit /b %errorlevel%

#__SH__
# ---------------------------------------------------------------------------
# cmd + POSIX sh 双解释器脚本：
#   Windows : scripts\package.bat [版本号]
#   Linux   : sh scripts/package.bat [版本号]
# 打包逻辑只有一份（tools/package_source.ps1），Linux 需要安装 PowerShell 7（pwsh）。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
cd "$ROOT" || exit 1

PS=$(command -v pwsh 2>/dev/null || true)
if [ -z "$PS" ]; then
    PS=$(command -v powershell 2>/dev/null || true)
fi
if [ -z "$PS" ]; then
    echo "[ERROR] 未找到 PowerShell，无法打包。" >&2
    echo "        Linux 请先安装 PowerShell 7（pwsh）：https://aka.ms/powershell" >&2
    exit 1
fi

mkdir -p out/pack
if [ -n "${1:-}" ]; then
    "$PS" -NoProfile -File tools/package_source.ps1 -OutputDir out/pack -Version "$1"
else
    "$PS" -NoProfile -File tools/package_source.ps1 -OutputDir out/pack
fi
exit $?
