:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  chcp 必须在任何中文（多字节）内容之前执行：cmd 切换代码页会重定位批处理读取位置，
REM  若前面已有多字节内容，切换后可能错位跳行（本地与 CI 初始代码页不同，症状不稳定）。
REM  gen_docs.bat  --  生成 Doxygen API 文档（中文 HTML）
REM
REM  用法（Windows）：scripts\gen_docs.bat
REM  用法（Linux）  ：sh scripts/gen_docs.bat
REM
REM  验收：退出码 0 且 out/doxygen/doxygen_warnings.log 为 0 字节
REM        （WARN_AS_ERROR 已开启）。产物：docs/html/index.html
REM  可用环境变量 DOXYGEN 指定 doxygen 可执行文件路径。
REM ============================================================================
setlocal
if not defined DOXYGEN set "DOXYGEN=D:\autotools\doxygen-1.11.0\doxygen.exe"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "LOGFILE=%ROOT%\out\doxygen\doxygen_warnings.log"

if not exist "%DOXYGEN%" (
    echo [ERROR] doxygen not found: %DOXYGEN%
    exit /b 2
)

pushd "%ROOT%"
REM 告警日志是生成物，统一放 out\ 下
if not exist "out\doxygen" md "out\doxygen" 2>nul
"%DOXYGEN%" Doxyfile
if errorlevel 1 (
    echo.
    echo [FAILED] doxygen reported warnings, see %LOGFILE%
    if exist "%LOGFILE%" type "%LOGFILE%"
    popd
    exit /b 1
)

echo.
echo [OK] html generated: %ROOT%\docs\html\index.html
start "" "%ROOT%\docs\html\index.html"
popd
exit /b 0

#__SH__
# ---------------------------------------------------------------------------
# cmd + POSIX sh 双解释器脚本：
#   Windows : scripts\gen_docs.bat
#   Linux   : sh scripts/gen_docs.bat
# Linux 用 PATH 里的 doxygen（或环境变量 DOXYGEN 指定的路径），不自动打开浏览器。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
cd "$ROOT" || exit 1

DOXYGEN_BIN=${DOXYGEN:-doxygen}
if ! command -v "$DOXYGEN_BIN" >/dev/null 2>&1; then
    echo "[ERROR] doxygen not found: $DOXYGEN_BIN" >&2
    echo "        请安装 doxygen（如 sudo apt install doxygen），或用 DOXYGEN=/path/to/doxygen 指定。" >&2
    exit 2
fi

mkdir -p out/doxygen
if ! "$DOXYGEN_BIN" Doxyfile; then
    echo ""
    echo "[FAILED] doxygen reported warnings, see out/doxygen/doxygen_warnings.log"
    [ -f out/doxygen/doxygen_warnings.log ] && cat out/doxygen/doxygen_warnings.log
    exit 1
fi

echo ""
echo "[OK] html generated: $ROOT/docs/html/index.html"
