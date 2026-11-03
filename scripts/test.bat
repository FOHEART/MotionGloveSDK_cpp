:; exec sh -c "$(sed -n '/^#__SH__/,$p' "$0" | tr -d '\r')" "$0" "$@" #
@echo off
chcp 65001 >nul
REM  chcp 必须在任何中文（多字节）内容之前执行：cmd 切换代码页会重定位批处理读取位置，
REM  若前面已有多字节内容，切换后可能错位跳行（本地与 CI 初始代码页不同，症状不稳定）。
REM  test.bat -- 构建并运行单元测试（GoogleTest / ctest）
REM
REM  用法（Windows）：
REM    scripts\test.bat                  默认 Debug，跑全部用例
REM    scripts\test.bat Release          指定配置
REM    scripts\test.bat Debug Unit.*     只跑匹配的用例（gtest 过滤）
REM
REM  用法（Linux）：
REM    sh scripts/test.bat [Debug / Release] [正则过滤]
REM
REM  构建树：out\build\tdd-tests，ctest 日志：out\build\tdd-tests\Testing
REM  Windows 逻辑在 tools\build_and_test.bat（会先 call vcvars64 再 configure）。
REM ============================================================================
setlocal
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
call "%ROOT%\tools\build_and_test.bat" %*
exit /b %errorlevel%

#__SH__
# ---------------------------------------------------------------------------
# cmd + POSIX sh 双解释器脚本：
#   Windows : scripts\test.bat [Debug|Release] [过滤]
#   Linux   : sh scripts/test.bat [Debug|Release] [过滤]
# Linux 不需要 vcvars，直接用 CMake 默认生成器（Make/Ninja）构建测试目标。
# ---------------------------------------------------------------------------
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd) || exit 1
cd "$ROOT" || exit 1

CONFIG=${1:-Debug}
FILTER=${2:-}
BUILD="out/build/tdd-tests"
JOBS=$( (command -v nproc >/dev/null 2>&1 && nproc) || echo 4 )

echo "[1/3] configure: $BUILD"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE="$CONFIG" || { echo "[ERROR] cmake configure failed"; exit 1; }

echo "[2/3] build: mgsdk_unit_tests [$CONFIG]"
cmake --build "$BUILD" --target mgsdk_unit_tests -j"$JOBS" || { echo "[ERROR] build failed"; exit 1; }

echo "[3/3] ctest [$CONFIG]"
cd "$BUILD" || exit 1
if [ -n "$FILTER" ]; then
    ctest --output-on-failure -R "$FILTER"
else
    ctest --output-on-failure
fi
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "[FAILED] ctest exit code = $rc"
    exit "$rc"
fi
echo "[OK] all tests passed"
