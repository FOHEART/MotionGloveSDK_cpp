@echo off
REM ============================================================================
REM  gen_docs.bat  --  生成 Doxygen API 文档（中文 HTML）
REM
REM  用法：scripts\gen_docs.bat
REM  验收：退出码 0 且 out\doxygen\doxygen_warnings.log 为 0 字节
REM        （WARN_AS_ERROR 已开启）。产物：docs\html\index.html
REM ============================================================================
setlocal
chcp 65001 >nul
set "DOXYGEN=D:\autotools\doxygen-1.11.0\doxygen.exe"
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
endlocal
