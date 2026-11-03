:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  chcp 必须在任何中文（多字节）内容之前执行：cmd 切换代码页会重定位批处理读取位置，
REM  若前面已有多字节内容，切换后可能错位跳行（本地与 CI 初始代码页不同，症状不稳定）。
REM  deploy.bat -- 安装某个配置的构建树到 out\install，并汇总可运行示例
REM
REM  用法（Windows）：
REM    scripts\deploy.bat Debug
REM    scripts\deploy.bat Release
REM    scripts\deploy.bat all
REM    scripts\deploy.bat Debug out\build\x64-Debug   （可选：显式指定构建树）
REM
REM  用法（Linux）：
REM    sh scripts/deploy.bat Debug [out/build/x64-Debug]
REM
REM  产物：
REM    out\install\x64-<配置>\          库 + 头文件
REM    out\install_example\x64-<配置>\  示例 + 运行库，可直接运行
REM ============================================================================
setlocal EnableDelayedExpansion
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
cd /d "%ROOT%"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=all"
if /i "%CONFIG%"=="all" (
    call "%~f0" Debug
    if errorlevel 1 exit /b 1
    call "%~f0" Release
    exit /b !errorlevel!
)

set "BDIR=%~2"
if "%BDIR%"=="" set "BDIR=out\build\x64-%CONFIG%"
if not exist "%BDIR%\CMakeCache.txt" (
    echo [ERROR] 未找到构建树 %BDIR%，请先运行 scripts\build.bat。
    exit /b 1
)

set "INSTALL=out\install\x64-%CONFIG%"
set "EXAMPLES=out\install_example\x64-%CONFIG%"
set "BIN=%BDIR%\%CONFIG%"

echo   -^> 安装到 %INSTALL%
cmake --install "%BDIR%" --config %CONFIG% --prefix "%INSTALL%"
if errorlevel 1 (
    echo Install failed, exit code: !errorlevel!
    exit /b 1
)

echo   -^> 汇总示例到 %EXAMPLES%
if exist "%EXAMPLES%" rd /s /q "%EXAMPLES%"
md "%EXAMPLES%" 2>nul
copy /y "%BIN%\*.exe" "%EXAMPLES%\" >nul 2>nul
if errorlevel 1 (
    echo [WARN] %BIN% 下没有示例可执行文件，跳过示例汇总。
    exit /b 0
)
copy /y "%BIN%\*.dll" "%EXAMPLES%\" >nul 2>nul

set "EXE_COUNT=0"
set "DLL_COUNT=0"
for %%F in ("%EXAMPLES%\*.exe") do set /a EXE_COUNT+=1
for %%F in ("%EXAMPLES%\*.dll") do set /a DLL_COUNT+=1
echo      %EXAMPLES%：!EXE_COUNT! 个示例，!DLL_COUNT! 个 DLL
exit /b 0

#__SH__
# ---------------------------------------------------------------------------
# cmd + POSIX sh 双解释器脚本：
#   Windows : scripts\deploy.bat [Debug|Release|all] [构建树]
#   Linux   : sh scripts/deploy.bat [Debug|Release|all] [build/x64-Debug]
# Linux 下示例产物没有 .exe 后缀、运行库是 .so，因此单独处理。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
cd "$ROOT" || exit 1

CONFIG=${1:-all}
if [ "$CONFIG" = "all" ]; then
    sh "$(dirname "$0")/deploy.bat" Debug || exit 1
    sh "$(dirname "$0")/deploy.bat" Release || exit 1
    exit 0
fi

BDIR=${2:-out/build/x64-$CONFIG}
if [ ! -f "$BDIR/CMakeCache.txt" ]; then
    echo "[ERROR] 未找到构建树 $BDIR，请先运行 sh scripts/build.bat。"
    exit 1
fi

INSTALL="out/install/x64-$CONFIG"
EXAMPLES="out/install_example/x64-$CONFIG"

echo "  -> 安装到 $INSTALL"
cmake --install "$BDIR" --prefix "$INSTALL" || { echo "[ERROR] install failed"; exit 1; }

echo "  -> 汇总示例到 $EXAMPLES"
rm -rf "$EXAMPLES"
mkdir -p "$EXAMPLES"

# 单配置生成器的产物在构建树根目录，多配置生成器在 <Config>/ 子目录
BIN="$BDIR/$CONFIG"
[ -d "$BIN" ] || BIN="$BDIR"

count=0
for f in "$BIN"/motionGloveSDK_example*; do
    [ -f "$f" ] || continue
    cp "$f" "$EXAMPLES/" || exit 1
    count=$((count + 1))
done
if [ "$count" -eq 0 ]; then
    echo "[WARN] $BIN 下没有示例可执行文件，跳过示例汇总。"
    exit 0
fi

# 运行库：构建树里的 .so + 安装目录 lib 下的 .so，一并拷到示例目录
for f in "$BIN"/*.so "$BIN"/*.so.*; do
    [ -f "$f" ] && cp "$f" "$EXAMPLES/" 2>/dev/null
done
for f in "$INSTALL"/lib/*.so "$INSTALL"/lib/*.so.*; do
    [ -f "$f" ] && cp "$f" "$EXAMPLES/" 2>/dev/null
done

echo "     $EXAMPLES：$count 个示例，$(find "$EXAMPLES" -name '*.so*' | wc -l | tr -d ' ') 个共享库"
echo "     运行前可执行：export LD_LIBRARY_PATH=\"\$PWD\""
