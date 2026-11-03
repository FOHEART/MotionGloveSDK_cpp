@echo off
REM ============================================================================
REM  test.bat -- 构建并运行单元测试（GoogleTest / ctest）
REM
REM  用法：
REM    scripts\test.bat                  默认 Debug，跑全部用例
REM    scripts\test.bat Release          指定配置
REM    scripts\test.bat Debug Unit.*     只跑匹配的用例（gtest 过滤）
REM
REM  构建树：out\build\tdd-tests，ctest 日志：out\build\tdd-tests\Testing
REM  逻辑在 tools\build_and_test.bat（会先 call vcvars64 再 configure）。
REM ============================================================================
setlocal
chcp 65001 >nul
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
call "%ROOT%\tools\build_and_test.bat" %*
exit /b %errorlevel%
