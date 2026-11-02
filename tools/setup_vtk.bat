@echo off
REM ============================================================================
REM  setup_vtk.bat -- 为 motionGloveSDK_example_vtk 准备 VTK 9.x（conda-forge 预编译）
REM
REM  由构建脚本在自身进程内 call 使用，成功返回 0 并导出：
REM      VTK_DIR            VTK 的 CMake 配置目录（含 vtk-config.cmake）
REM      VTK_CMAKE_PREFIX   需要追加到 CMAKE_PREFIX_PATH 的前缀（VTK 依赖库）
REM      VTK_BIN_DIR        VTK 及其依赖的 DLL 目录（运行示例时加入 PATH）
REM
REM  默认按"最小依赖"安装：conda-forge 的 vtk 元包会把 python 绑定、Qt6、boost 头文件、
REM  MKL、ffmpeg 等一并拖下来（约 2.2 GB / 48000+ 文件），而示例只用 C++ 渲染的十来个模块。
REM  最小安装改成「显式第三方依赖 + vtk-base/vtk-io-ffmpeg 不带依赖」两步，
REM  装好约 490 MB / 7200 文件，配置、编译、运行结果与完整安装完全一致。
REM
REM  可选环境变量（均可不设）：
REM      VTK_DIR                 已安装 VTK 的 CMake 配置目录，直接复用
REM      VTK_PREFIX              本脚本安装 VTK 的前缀（默认 third_party\vtk）
REM      VTK_CONDA_CHANNEL       conda 频道（默认清华 conda-forge 镜像）
REM      VTK_FULL=1              安装完整的 vtk 元包（含 python 绑定 / Qt6 / ffmpeg），
REM                              仅在需要这些模块时使用；默认走最小安装
REM      VTK_CONDA_SPEC          VTK_FULL=1 时安装的包（默认 vtk）
REM      VTK_MINIMAL_SPEC        最小安装的 VTK 包（默认 vtk-base=9.7 vtk-io-ffmpeg=9.7）
REM      VTK_MINIMAL_DEPS        最小安装的第三方依赖包清单（默认值见下方 set）
REM      VTK_FORCE_REINSTALL=1   先删除已有前缀再安装（在最小/完整模式之间切换时用）
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
if "%VTK_MINIMAL_SPEC%"=="" set "VTK_MINIMAL_SPEC=vtk-base=9.7 vtk-io-ffmpeg=9.7"
REM 最小安装的第三方依赖：与 motionGloveSDK_example_vtk 的模块闭包一一对应。
REM  -devel 包提供 CMake 配置与导入库（tbb-devel / liblzma-devel），缺了 find_package(VTK) 会失败。
if "%VTK_MINIMAL_DEPS%"=="" set "VTK_MINIMAL_DEPS=freetype libpng libjpeg-turbo libtiff zlib liblzma liblzma-devel zstd lz4-c lerc libdeflate jsoncpp pugixml fmt tbb tbb-devel double-conversion exprtk fast_float utfcpp nlohmann_json cli11 gl2ps libharu libsqlite libogg libtheora expat libxml2 vc14_runtime"
if "%VTK_MICROMAMBA_VERSION%"=="" set "VTK_MICROMAMBA_VERSION=2.10.0"

set "VTK_CONFIG="
REM 强制重装：先删前缀，避免落到下面的"复用已有安装"分支
if "%VTK_FORCE_REINSTALL%"=="1" if exist "%VTK_PREFIX%" (
    echo [VTK] VTK_FORCE_REINSTALL=1，删除已有前缀：%VTK_PREFIX%
    rd /s /q "%VTK_PREFIX%"
)
if not "%VTK_DIR%"=="" if exist "%VTK_DIR%\vtk-config.cmake" set "VTK_CONFIG=%VTK_DIR%"
set "VTK_USER_DIR=%VTK_DIR%"
if not defined VTK_CONFIG if exist "%VTK_PREFIX%\Library\lib\cmake" (
    for /d %%D in ("%VTK_PREFIX%\Library\lib\cmake\vtk-*") do (
        if exist "%%D\vtk-config.cmake" set "VTK_CONFIG=%%D"
    )
)
if defined VTK_CONFIG (
    REM 已安装的是完整版（含 Qt6 / python 绑定 / ffmpeg），提示可换成最小安装
    if "%VTK_FULL%"=="1" goto :report
    if not "%VTK_USER_DIR%"=="" goto :report
    if not exist "%VTK_PREFIX%\Library\lib\cmake\Qt6" goto :report
    echo [VTK] 注意：当前是完整版 VTK（含 Qt6 / python 绑定 / ffmpeg），占用空间远大于最小安装。
    echo        精简用法：set VTK_FORCE_REINSTALL=1 后重跑本脚本（约 490 MB / 7200 文件）。
    echo        确实需要完整版：set VTK_FULL=1 可关闭本提示。
    goto :report
)

if "%VTK_SKIP_INSTALL%"=="1" (
    echo [VTK] VTK_SKIP_INSTALL=1 且未找到已安装的 VTK，跳过自动安装。
    goto :fail
)

echo [VTK] 未找到 VTK，开始自动安装（%VTK_PREFIX%）...
REM 完整安装（含 Qt6 / python 绑定）无法在原前缀上"减肥"，需重装
if not "%VTK_FULL%"=="1" if exist "%VTK_PREFIX%\conda-meta" if exist "%VTK_PREFIX%\Library\lib\cmake\Qt6" (
    echo [VTK] 检测到已安装的完整版 VTK（含 Qt6 / python 绑定），占用空间远大于最小安装。
    echo        改为最小安装请先删除再重跑：rd /s /q "%VTK_PREFIX%"
    echo        或设 VTK_FORCE_REINSTALL=1 让本脚本自动删除。
    echo        确实需要完整 VTK 请设 VTK_FULL=1。
    goto :fail
)

call :ensure_micromamba
if errorlevel 1 (
    echo [VTK] 获取 micromamba 失败，请检查网络或自行设置 VTK_DIR。
    goto :fail
)

set "MAMBA_ROOT_PREFIX=%ROOT%\third_party\micromamba-root"
if "%VTK_ALLOW_INSECURE_SSL%"=="1" set "MAMBA_SSL_VERIFY=false"

if "%VTK_FULL%"=="1" (
    if exist "%VTK_PREFIX%\conda-meta" (
        "%MAMBA%" install -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" "%VTK_CONDA_SPEC%"
    ) else (
        "%MAMBA%" create -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" "%VTK_CONDA_SPEC%"
    )
) else (
    REM 两步走：先按正常依赖关系装第三方运行库（自动带上 brotli / libwebp 等传递依赖），
    REM 再以 --no-deps 装 VTK 本体，跳过它声明的 python / Qt6 / numpy / hdf5 / boost 等。
    if exist "%VTK_PREFIX%\conda-meta" (set "_vtk_mode=install") else (set "_vtk_mode=create")
    REM 目录存在但没有环境元数据（上次安装被中断）：先清空再 create
    if "!_vtk_mode!"=="create" if exist "%VTK_PREFIX%" rd /s /q "%VTK_PREFIX%"
    echo [VTK] 最小安装：第三方依赖 -^> %VTK_PREFIX%
    "%MAMBA%" !_vtk_mode! -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" %VTK_MINIMAL_DEPS%
    if not errorlevel 1 (
        echo [VTK] 最小安装：%VTK_MINIMAL_SPEC%（--no-deps）
        "%MAMBA%" install -y -p "%VTK_PREFIX%" -c "%VTK_CONDA_CHANNEL%" --no-deps %VTK_MINIMAL_SPEC%
    ) else (
        echo [VTK] 第三方依赖安装失败。
        goto :install_fail
    )
)
if errorlevel 1 goto :install_fail
goto :verify

:install_fail
echo [VTK] conda 安装失败。可尝试：
echo         set VTK_CONDA_CHANNEL=https://conda.anaconda.org/conda-forge
echo         set VTK_ALLOW_INSECURE_SSL=1
echo         set VTK_SKIP_INSTALL=1
echo         set VTK_FULL=1        （退回完整安装）
goto :fail

:verify
for /d %%D in ("%VTK_PREFIX%\Library\lib\cmake\vtk-*") do (
    if exist "%%D\vtk-config.cmake" set "VTK_CONFIG=%%D"
)
if not defined VTK_CONFIG (
    echo [VTK] 安装完成，但在 %VTK_PREFIX%\Library\lib\cmake 下找不到 vtk-config.cmake。
    goto :fail
)
REM 最小安装的关键文件自检：缺任何一个都会让 find_package(VTK) 或链接失败
if not "%VTK_FULL%"=="1" (
    set "_vtk_missing="
    if not exist "%VTK_PREFIX%\Library\bin\vtkCommonCore-*.dll" set "_vtk_missing=!_vtk_missing! vtk-modules"
    if not exist "%VTK_PREFIX%\Library\lib\cmake\TBB\TBBConfig.cmake" set "_vtk_missing=!_vtk_missing! tbb-devel"
    if not exist "%VTK_PREFIX%\Library\lib\lzma.lib" set "_vtk_missing=!_vtk_missing! liblzma-devel"
    if not exist "%VTK_PREFIX%\Library\include\expat.h" set "_vtk_missing=!_vtk_missing! expat"
    if defined _vtk_missing (
        echo [VTK] 最小安装缺少关键文件：!_vtk_missing!
        echo        请检查 VTK_MINIMAL_DEPS 是否被覆盖，或设 VTK_FORCE_REINSTALL=1 重装。
        goto :fail
    )
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
