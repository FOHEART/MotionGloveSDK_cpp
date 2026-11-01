@echo off
REM ============================================================================
REM  gen_docs.bat  --  生成 Doxygen API 文档（中文 HTML）
REM
REM  用法：gen_docs.bat
REM  验收：退出码 0 且 doxygen_warnings.log 为 0 字节（WARN_AS_ERROR 已开启）。
REM        产物：docs\html\index.html
REM ============================================================================
setlocal
chcp 65001 >nul
set "DOXYGEN=D:\autotools\doxygen-1.11.0\doxygen.exe"
set "ROOT=%~dp0"

if not exist "%DOXYGEN%" (
    echo [ERROR] doxygen not found: %DOXYGEN%
    exit /b 2
)

pushd "%ROOT%"
"%DOXYGEN%" Doxyfile
if errorlevel 1 (
    echo.
    echo [FAILED] doxygen reported warnings, see doxygen_warnings.log
    type doxygen_warnings.log
    popd
    exit /b 1
)

echo.
echo [OK] html generated: %ROOT%docs\html\index.html
start "" "%ROOT%docs\html\index.html"
popd
endlocal