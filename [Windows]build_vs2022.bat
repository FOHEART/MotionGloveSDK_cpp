@echo off
REM ============================================================================
REM  [Windows]build_vs2022.bat -- 兼容入口（双击可用）
REM  实际构建逻辑在 scripts\build.bat，产物统一在 out\ 下：
REM    out\build\x64-<配置>\            构建树
REM    out\install\x64-<配置>\          库 + 头文件
REM    out\install_example\x64-<配置>\  示例 + 运行库
REM ============================================================================
call "%~dp0scripts\build.bat" 2022 %*
exit /b %errorlevel%
