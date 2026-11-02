@echo off
REM ============================================================================
REM  setup_vtk.bat -- 为 motionGloveSDK_example_vtk 准备 VTK 9.x（conda-forge 预编译）
REM
REM  由构建脚本在自身进程内 call 使用，成功返回 0 并导出：
REM      VTK_DIR            VTK 的 CMake 配置目录（含 vtk-config.cmake）
REM      VTK_CMAKE_PREFIX   需要追加到 CMAKE_PREFIX_PATH 的前缀（VTK 依赖库）
REM      VTK_BIN_DIR        VTK 及其依赖的 DLL 目录（运行示例时加入 PATH）
REM
REM  可选环境变量（均可不设）：
REM      VTK_DIR                 已安装 VTK 的 CMake 配置目录，直接复用
REM      VTK_PREFIX              本脚本安装 VTK 的前缀（默认 third_party\vtk）
REM      VTK_CONDA_CHANNEL       conda 频道（默认清华 conda-forge 镜像）
REM      VTK_CONDA_SPEC          安装的包（默认 vtk，含 vtk-base + vtk-io-ffmpeg）
REM      VTK_MICROMAMBA_VERSION  下载的 micromamba 版本（默认 2.10.0）
REM      VTK_SKIP_INSTALL=1      只复用已安装的 VTK，缺省时不联网安装
REM      VTK_KEEP_PKG_CACHE=1    安装后保留 conda 包缓存（默认清理，可省数 GB）
REM      VTK_ALLOW_INSECURE_SSL=1 下载时关闭 SSL 证书校验（证书吊销列表不可达时用）
REM ============================================================================
setlocal EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
if "%VTK_PREFIX%"=="" set "VTK_PREFIX=%ROOT%\third_party\vtk"
if "%VTK_CONDA_CHANNEL%"=="" set "VTK_CONDA_CHANNEL=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge"
if "%VTK_CONDA_SPEC%"=="" set "VTK_CONDA_SPEC=vtk"
if "%VTK_MICROMAMBA_VERSION%"=="" set "VTK_MICROMAMBA_VERSION=2.10.0"

set "VTK_CONFIG="
if not "%VTK_DIR%"=="" if exist "%VTK_DIR%\vtk-config.cmake" set "VTK_CONFIG=%VTK_DIR%"
if not defined VTK_CONFIG if exist "%VTK_PREFIX%\Library\lib\cmake" (
    for /d %%D in ("%VTK_PREFIX%\Library\lib\cmake\vtk-*") do (
        if exist "%%D\vtk-config.cmake" set "VTK_CONFIG=%%D"
    )
)
if defined VTK_CONFIG goto :report

if "%VTK_SKIP_INSTALL%"=="1" (
    echo [VTK] VTK_SKIP_INSTALL=1 且未找到已安装的 VTK，跳过自动安装。
    goto :fail
)

echo [VTK] 未找到 VTK，开始自动安装（%VTK_CONDA_SPEC% -^> %VTK_PREFIX%）...
call :ensure_micromamba
if errorlevel 1 (
    echo [VTK] 获取 micromamba 失败，请检查网络或自行设置 VTK_DIR。
    goto :fail
)

set "MAMBA_ROOT_PREFIX=%ROOT%\third_party\micromamba-root"
if "%VTK_ALLOW_INSECURE_SSL%"=="1" set "MAMBA_SSL_VERIFY=false"

if exist "%VTK_PREFIX%\conda-meta" (
    "%MAMBA%" install -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" "%VTK_CONDA_SPEC%"
) else (
    "%MAMBA%" create -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" "%VTK_CONDA_SPEC%"
)
if errorlevel 1 (
    echo [VTK] conda 安装失败。可尝试：
    echo         set VTK_CONDA_CHANNEL=https://conda.anaconda.org/conda-forge
    echo         set VTK_ALLOW_INSECURE_SSL=1
    echo         set VTK_SKIP_INSTALL=1
    goto :fail
)

for /d %%D in ("%VTK_PREFIX%\Library\lib\cmake\vtk-*") do (
    if exist "%%D\vtk-config.cmake" set "VTK_CONFIG=%%D"
)
if not defined VTK_CONFIG (
    echo [VTK] 安装完成，但在 %VTK_PREFIX%\Library\lib\cmake 下找不到 vtk-config.cmake。
    goto :fail
)

REM conda 包缓存体积可达数 GB，装好后默认清理（VTK_KEEP_PKG_CACHE=1 可保留）
if not "%VTK_KEEP_PKG_CACHE%"=="1" (
    "%MAMBA%" clean --all --yes >nul 2>nul
)

:report
REM vtk-config.cmake 位于 <prefix>\Library\lib\cmake\vtk-X.Y，回溯三级即 VTK 前缀。
for %%I in ("%VTK_CONFIG%\..\..\..") do set "VTK_CMAKE_PREFIX=%%~fI"
set "VTK_BIN="
for %%I in ("%VTK_CMAKE_PREFIX%\bin") do if exist "%%~fI" set "VTK_BIN=%%~fI"
echo [VTK] VTK_DIR         = %VTK_CONFIG%
if defined VTK_BIN echo [VTK] VTK_BIN_DIR     = %VTK_BIN%
endlocal & set "VTK_DIR=%VTK_CONFIG%" & set "VTK_CMAKE_PREFIX=%VTK_CMAKE_PREFIX%" & set "VTK_BIN_DIR=%VTK_BIN%" & exit /b 0

:fail
endlocal & set "VTK_DIR=" & set "VTK_CMAKE_PREFIX=" & set "VTK_BIN_DIR=" & exit /b 1

REM ── 获取 micromamba（conda-forge 的包解包后即单文件可执行程序）─────────────
:ensure_micromamba
set "MAMBA=%ROOT%\third_party\micromamba\Library\bin\micromamba.exe"
if exist "%MAMBA%" exit /b 0
if exist "%ROOT%\third_party\micromamba\micromamba.exe" (
    set "MAMBA=%ROOT%\third_party\micromamba\micromamba.exe"
    exit /b 0
)
for %%I in (micromamba.exe) do if not "%%~$PATH:I"=="" (
    set "MAMBA=%%~$PATH:I"
    exit /b 0
)

where tar.exe >nul 2>nul
if errorlevel 1 (
    echo [VTK] 未找到 tar.exe，无法解包 micromamba。
    exit /b 1
)

set "MM_TMP=%TEMP%\mgsdk-micromamba"
if exist "%MM_TMP%" rd /s /q "%MM_TMP%"
md "%MM_TMP%" 2>nul
md "%ROOT%\third_party\micromamba" 2>nul

call :download "%VTK_CONDA_CHANNEL%/win-64/micromamba-%VTK_MICROMAMBA_VERSION%-0.tar.bz2" "%MM_TMP%\micromamba.tar.bz2"
if errorlevel 1 (
    call :download "https://micro.mamba.pm/api/micromamba/win-64/latest" "%MM_TMP%\micromamba.tar.bz2"
    if errorlevel 1 exit /b 1
)

tar.exe -xjf "%MM_TMP%\micromamba.tar.bz2" -C "%ROOT%\third_party\micromamba"
if errorlevel 1 exit /b 1

if exist "%ROOT%\third_party\micromamba\Library\bin\micromamba.exe" (
    set "MAMBA=%ROOT%\third_party\micromamba\Library\bin\micromamba.exe"
) else if exist "%ROOT%\third_party\micromamba\micromamba.exe" (
    set "MAMBA=%ROOT%\third_party\micromamba\micromamba.exe"
) else (
    exit /b 1
)
"%MAMBA%" --version >nul 2>nul
if errorlevel 1 exit /b 1
echo [VTK] micromamba     = %MAMBA%
exit /b 0

REM ── 下载：curl → curl --ssl-no-revoke → PowerShell 逐级退化 ────────────────
:download
set "DL_URL=%~1"
set "DL_DST=%~2"
curl.exe -fsSL -m 900 -o "%DL_DST%" "%DL_URL%" >nul 2>nul
if not errorlevel 1 exit /b 0
curl.exe -fsSL --ssl-no-revoke -m 900 -o "%DL_DST%" "%DL_URL%" >nul 2>nul
if not errorlevel 1 exit /b 0
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -UseBasicParsing -Uri '%DL_URL%' -OutFile '%DL_DST%' } catch { exit 1 }" >nul 2>nul
if not errorlevel 1 exit /b 0
exit /b 1
