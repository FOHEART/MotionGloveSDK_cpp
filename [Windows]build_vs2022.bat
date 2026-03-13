@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo [1/4] Generating Visual Studio 2022 project (Debug)...
cmake -S . -B out/build/x64-Debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug"
if %errorlevel% neq 0 (
    echo CMake configuration failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo [2/4] Building Debug...
cmake --build out/build/x64-Debug --config Debug
if %errorlevel% neq 0 (
    echo Build failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)

cmake --install out/build/x64-Debug --config Debug --prefix out/install/x64-Debug
if %errorlevel% neq 0 (
    echo Install failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo [3/4] Generating Visual Studio 2022 project (Release)...
cmake -S . -B out/build/x64-Release -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release"
if %errorlevel% neq 0 (
    echo CMake configuration failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo [4/4] Building Release...
cmake --build out/build/x64-Release --config Release
if %errorlevel% neq 0 (
    echo Build failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)

cmake --install out/build/x64-Release --config Release --prefix out/install/x64-Release
if %errorlevel% neq 0 (
    echo Install failed, exit code: %errorlevel%
    pause
    exit /b %errorlevel%
)


echo All builds completed.
pause
