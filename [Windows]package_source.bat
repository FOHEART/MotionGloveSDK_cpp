@echo off
chcp 65001 >nul
cd /d "%~dp0"

REM ============================================================================
REM  把当前工程打成可分发给客户的源码包：MotionGloveSDK_cpp_v<版本号>.zip
REM
REM  逻辑在 tools\package_source.ps1 中，这里只做双击启动的入口。
REM  分发包里不含 .git、.github、git 拉取脚本、构建产物与内部资料。
REM
REM  可选参数：
REM    [Windows]package_source.bat                 版本号取自版本头文件
REM    [Windows]package_source.bat 0.2.2           指定版本号
REM
REM  注意：请在“干净”的工作区执行（先提交改动），否则包内会带上未提交的文件。
REM ============================================================================

set "VERSION=%~1"
if "%VERSION%"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package_source.ps1"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package_source.ps1" -Version "%VERSION%"
)
exit /b %errorlevel%
