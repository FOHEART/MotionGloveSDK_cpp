@echo off
REM ============================================================================
REM  build_and_test.bat  --  MotionGloveSDK 单元测试：配置 + 构建 + ctest
REM
REM  用法：
REM    tools\build_and_test.bat                 默认 Debug，构建并跑全部用例
REM    tools\build_and_test.bat Release         指定配置
REM    tools\build_and_test.bat Debug Unit.*    只跑匹配的用例（gtest 过滤）
REM
REM  说明：先加载 VS2022 开发环境（vcvars64），否则 cmake 找不到 MSVC。
REM        构建目录与 CMakeSettings 生成的目录隔离，互不影响。
REM ============================================================================
setlocal enabledelayedexpansion
chcp 65001 >nul

set "ROOT=%~dp0.."
set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
set "FILTER=%~2"
set "BUILD=%ROOT%\out\build\tdd-tests"

if not exist "%VSDEV%" (
    echo [ERROR] vcvars64.bat not found: %VSDEV%
    exit /b 2
)
call "%VSDEV%" >nul
if errorlevel 1 (
    echo [ERROR] failed to load Visual Studio environment
    exit /b 2
)

echo [1/3] configure: %BUILD%
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug;Release"
if errorlevel 1 (
    echo [ERROR] cmake configure failed
    exit /b 1
)

echo [2/3] build: mgsdk_unit_tests [%CONFIG%]
cmake --build "%BUILD%" --config %CONFIG% --target mgsdk_unit_tests
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

echo [3/3] ctest [%CONFIG%]
pushd "%BUILD%"
if "%FILTER%"=="" (
    ctest -C %CONFIG% --output-on-failure
) else (
    ctest -C %CONFIG% --output-on-failure -R "%FILTER%"
)
set "CTEST_RC=%errorlevel%"
popd

if not "%CTEST_RC%"=="0" (
    echo [FAILED] ctest exit code = %CTEST_RC%
    exit /b %CTEST_RC%
)
echo [OK] all tests passed
exit /b 0
