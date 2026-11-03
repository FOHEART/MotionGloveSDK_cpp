:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  chcp 必须在任何中文（多字节）内容之前执行：cmd 切换代码页会重定位批处理读取位置，
REM  若前面已有多字节内容，切换后可能错位跳行（本地与 CI 初始代码页不同，症状不稳定）。
REM  build.bat -- 一键构建（配置 + 编译 + 安装 + 汇总示例），只认 out\ 下的路径
REM
REM  用法（Windows）：
REM    scripts\build.bat                 Visual Studio 2022，Debug + Release
REM    scripts\build.bat 2026            Visual Studio 2026，Debug + Release
REM    scripts\build.bat 2022 Release    只构建 Release
REM
REM  用法（Linux）：
REM    sh scripts/build.bat              Debug + Release（CMake 默认生成器）
REM    sh scripts/build.bat Release      只构建 Release
REM
REM  产物（全部在 out\ 下，源码树不留生成物）：
REM    out/build/x64-<配置>/             构建树
REM    out/install/x64-<配置>/           库 + 头文件
REM    out/install_example/x64-<配置>/   示例 + 运行库，可直接运行
REM ============================================================================
setlocal EnableDelayedExpansion
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
cd /d "%ROOT%"

set "VS_VER=2022"
set "CONFIG=all"
if not "%~1"=="" for %%A in (%*) do (
    if /i "%%~A"=="2022" set "VS_VER=2022"
    if /i "%%~A"=="2026" set "VS_VER=2026"
    if /i "%%~A"=="Debug" set "CONFIG=Debug"
    if /i "%%~A"=="Release" set "CONFIG=Release"
)

if "%VS_VER%"=="2026" (
    set "GENERATOR=Visual Studio 18 2026"
    set "VS_TAG=Visual Studio 2026"
) else (
    set "GENERATOR=Visual Studio 17 2022"
    set "VS_TAG=Visual Studio 2022"
)

set "VTK_ENABLED=0"
echo [1/3] 准备 VTK（可选，供 motionGloveSDK_example_vtk 使用）...
call "%ROOT%\tools\setup_vtk.bat"
if not errorlevel 1 (
    set "VTK_ENABLED=1"
    set "VTK_ARGS=-DBUILD_VTK_EXAMPLE=ON "-DVTK_DIR=!VTK_DIR!" "-DCMAKE_PREFIX_PATH=!VTK_CMAKE_PREFIX!""
) else (
    echo [WARN] 未获取到 VTK，本次构建将跳过 motionGloveSDK_example_vtk。
    set "VTK_ARGS=-DBUILD_VTK_EXAMPLE=OFF"
)

echo [2/3] 构建（%VS_TAG%，配置：%CONFIG%）...
if /i "%CONFIG%"=="all" (
    call :build_config Debug
    if errorlevel 1 goto :build_failed
    call :build_config Release
    if errorlevel 1 goto :build_failed
) else (
    call :build_config %CONFIG%
    if errorlevel 1 goto :build_failed
)

echo [3/3] 完成：out\build\x64-<配置> / out\install\x64-<配置> / out\install_example\x64-<配置>
exit /b 0

REM ── 配置 + 编译 + 安装 + 汇总指定配置 ─────────────────────────────────────
:build_config
set "CFG=%~1"
set "BDIR=out\build\x64-%CFG%"
echo   -^> 配置 %BDIR%
cmake -S "%ROOT%" -B "%BDIR%" -G "%GENERATOR%" -A x64 ^
    -DCMAKE_CONFIGURATION_TYPES="Debug;Release" ^
    -DCMAKE_INSTALL_PREFIX=out/install/x64-%CFG% %VTK_ARGS%
if errorlevel 1 (
    echo CMake configuration failed, exit code: !errorlevel!
    exit /b 1
)

echo   -^> 编译 %CFG%
cmake --build "%BDIR%" --config %CFG%
if not errorlevel 1 (
    call "%~dp0deploy.bat" %CFG%
    exit /b !errorlevel!
)

REM 构建失败时，若启用了 VTK，退回不编译 VTK 示例后重试
if not "%VTK_ENABLED%"=="1" (
    echo Build failed, exit code: !errorlevel!
    exit /b 1
)
echo [WARN] 构建失败，可能与预编译 VTK 的配置不兼容；改为不编译 VTK 示例后重试...
cmake -S "%ROOT%" -B "%BDIR%" -G "%GENERATOR%" -A x64 ^
    -DCMAKE_CONFIGURATION_TYPES="Debug;Release" ^
    -DCMAKE_INSTALL_PREFIX=out/install/x64-%CFG% -DBUILD_VTK_EXAMPLE=OFF
if errorlevel 1 (
    echo CMake configuration failed, exit code: !errorlevel!
    exit /b 1
)
cmake --build "%BDIR%" --config %CFG%
if errorlevel 1 (
    echo Build failed, exit code: !errorlevel!
    exit /b 1
)
echo [WARN] 已跳过 motionGloveSDK_example_vtk，其余目标构建成功。
call "%~dp0deploy.bat" %CFG%
exit /b !errorlevel!

:build_failed
echo.
echo 构建未完成，请检查上面的错误输出。
exit /b 1

#__SH__
# ---------------------------------------------------------------------------
# 本文件是 cmd + POSIX sh 双解释器脚本：
#   Windows : 双击，或 cmd 里运行 scripts\build.bat [2022|2026] [Debug|Release]
#   Linux   : sh scripts/build.bat [Debug|Release]
# 首行对 cmd 是一行标签（整行忽略），对 sh 是 exec：取 #__SH__ 之后的 POSIX 段去 \r
# 重新执行，因此 Windows 检出（CRLF）直接放进 WSL/Linux 也能跑。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
cd "$ROOT" || exit 1

CONFIGS=""
for arg in "$@"; do
    case "$arg" in
        [Dd]ebug)   CONFIGS="$CONFIGS Debug" ;;
        [Rr]elease) CONFIGS="$CONFIGS Release" ;;
        2022|2026)  echo "[INFO] Linux 下忽略参数 $arg（使用 CMake 默认生成器）" ;;
        *)          echo "[WARN] 忽略未知参数: $arg" ;;
    esac
done
[ -z "$CONFIGS" ] && CONFIGS="Debug Release"

JOBS=$( (command -v nproc >/dev/null 2>&1 && nproc) || echo 4 )

echo "[1/3] 准备 VTK（可选）..."
echo "      Linux 下由 CMake 自行查找 VTK；找不到时 CMakeLists 会自动关闭 VTK 示例。"

build_one() {
    cfg=$1
    bdir="out/build/x64-$cfg"
    inst="$ROOT/out/install/x64-$cfg"

    echo "[2/3] 构建 $cfg（$bdir）..."
    cmake -S "$ROOT" -B "$bdir" -DCMAKE_BUILD_TYPE="$cfg" -DCMAKE_INSTALL_PREFIX="$inst" || return 1
    if ! cmake --build "$bdir" -j"$JOBS"; then
        echo "[WARN] 构建失败，可能与 VTK 配置不兼容；改为不编译 VTK 示例后重试..."
        cmake -S "$ROOT" -B "$bdir" -DCMAKE_BUILD_TYPE="$cfg" \
              -DCMAKE_INSTALL_PREFIX="$inst" -DBUILD_VTK_EXAMPLE=OFF || return 1
        cmake --build "$bdir" -j"$JOBS" || return 1
    fi

    sh "$(dirname "$0")/deploy.bat" "$cfg" "$bdir" || return 1
}

for cfg in $CONFIGS; do
    build_one "$cfg" || { echo ""; echo "构建未完成，请检查上面的错误输出。"; exit 1; }
done

echo "[3/3] 完成：out/build/x64-{Debug,Release} / out/install/x64-* / out/install_example/x64-*"
