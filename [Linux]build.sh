#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

die() {
    echo "[ERROR] $1"
    exit 1
}

echo "[1/4] Configuring CMake (Debug)..."
cmake -S . -B out/build/x64-Debug \
    -DCMAKE_BUILD_TYPE=Debug \
    || die "CMake configuration failed."

echo "[2/4] Building Debug..."
cmake --build out/build/x64-Debug \
    || die "Build failed."

cmake --install out/build/x64-Debug \
    --prefix out/install/x64-Debug \
    || die "Install failed."

echo "[3/4] Configuring CMake (Release)..."
cmake -S . -B out/build/x64-Release \
    -DCMAKE_BUILD_TYPE=Release \
    || die "CMake configuration failed."

echo "[4/4] Building Release..."
cmake --build out/build/x64-Release \
    || die "Build failed."

cmake --install out/build/x64-Release \
    --prefix out/install/x64-Release \
    || die "Install failed."

echo ""
echo "All builds completed."
