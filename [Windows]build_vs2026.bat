@echo off
chcp 65001 >nul
cd /d "%~dp0"
setlocal EnableDelayedExpansion

set "GENERATOR=Visual Studio 18 2026"
set "VS_TAG=Visual Studio 2026"
set "VTK_ENABLED=0"

echo [1/4] 准备 VTK（供 motionGloveSDK_example_vtk 使用，可选）...
call "%~dp0tools\setup_vtk.bat"
if not errorlevel 1 (
    set "VTK_ENABLED=1"
    set "VTK_ARGS=-DBUILD_VTK_EXAMPLE=ON "-DVTK_DIR=!VTK_DIR!" "-DCMAKE_PREFIX_PATH=!VTK_CMAKE_PREFIX!""
) else (
    echo [WARN] 未获取到 VTK，本次构建将跳过 motionGloveSDK_example_vtk。
    set "VTK_ARGS=-DBUILD_VTK_EXAMPLE=OFF"
)

echo [2/4] Generating %VS_TAG% project (Debug)...
call :configure_and_build Debug x64-Debug
if errorlevel 1 goto :build_failed

cmake --install out/build/x64-Debug --config Debug --prefix out/install/x64-Debug
if errorlevel 1 (
    echo Install failed, exit code: !errorlevel!
    goto :build_failed
)

echo [3/4] Generating %VS_TAG% project (Release)...
call :configure_and_build Release x64-Release
if errorlevel 1 goto :build_failed

cmake --install out/build/x64-Release --config Release --prefix out/install/x64-Release
if errorlevel 1 (
    echo Install failed, exit code: !errorlevel!
    goto :build_failed
)

echo [4/4] All builds completed.
if "%VTK_ENABLED%"=="1" (
    echo VTK 示例输出：out\build\x64-Debug\Debug\ 与 out\build\x64-Release\Release\
    echo 依赖的 VTK 运行库已复制到同目录，可直接运行 motionGloveSDK_example_vtk.exe
) else (
    echo VTK 示例未编译。可参考 README「可选：VTK 3D 可视化示例」章节手动安装 VTK。
)
exit /b 0

REM ── 配置并构建指定配置；若 VTK 导致失败，则退回不编译 VTK 示例后重试 ──────
:configure_and_build
set "CFG=%~1"
set "BDIR=out/build/%~2"
cmake -S . -B "%BDIR%" -G "%GENERATOR%" -A x64 -DCMAKE_CONFIGURATION_TYPES="%CFG%" %VTK_ARGS%
if errorlevel 1 (
    echo CMake configuration failed, exit code: !errorlevel!
    exit /b 1
)

cmake --build "%BDIR%" --config %CFG%
if not errorlevel 1 exit /b 0

if not "%VTK_ENABLED%"=="1" (
    echo Build failed, exit code: !errorlevel!
    exit /b 1
)

echo [WARN] 构建失败，可能与预编译 VTK 的配置不兼容；改为不编译 VTK 示例后重试...
cmake -S . -B "%BDIR%" -G "%GENERATOR%" -A x64 -DCMAKE_CONFIGURATION_TYPES="%CFG%" -DBUILD_VTK_EXAMPLE=OFF
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
exit /b 0

:build_failed
echo.
echo 构建未完成，请检查上面的错误输出。
exit /b 1
