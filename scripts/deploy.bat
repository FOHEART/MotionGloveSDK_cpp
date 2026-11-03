@echo off
REM ============================================================================
REM  deploy.bat -- 安装某个配置的构建树到 out\install，并汇总可运行示例
REM
REM  用法：
REM    scripts\deploy.bat Debug
REM    scripts\deploy.bat Release
REM    scripts\deploy.bat all
REM    scripts\deploy.bat Debug out\build\x64-Debug   （可选：显式指定构建树）
REM
REM  产物：
REM    out\install\x64-<配置>\          库 + 头文件
REM    out\install_example\x64-<配置>\  示例 exe + DLL，可直接运行
REM ============================================================================
setlocal EnableDelayedExpansion
chcp 65001 >nul
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
