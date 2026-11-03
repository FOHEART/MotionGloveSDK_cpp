@echo off
REM ============================================================================
REM  package.bat -- 打源码分发包（含可选 preBuild 预编译产物）
REM
REM  用法：
REM    scripts\package.bat              版本号取自版本头文件
REM    scripts\package.bat 0.2.2        指定版本号
REM
REM  产物：out\pack\MotionGloveSDK_cpp_v<版本号>.zip
REM  逻辑在 tools\package_source.ps1，暂存目录也在 out\pack 下。
REM ============================================================================
setlocal
chcp 65001 >nul
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "OUTDIR=%ROOT%\out\pack"
if not exist "%OUTDIR%" md "%OUTDIR%" 2>nul

if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\package_source.ps1" -OutputDir "%OUTDIR%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\package_source.ps1" -OutputDir "%OUTDIR%" -Version "%~1"
)
exit /b %errorlevel%
