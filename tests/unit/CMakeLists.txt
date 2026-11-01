# ── MotionGloveSDK 单元测试（GoogleTest / TDD） ─────────────────────────────
#
# 设计要点（依据《TDD与GoogleTest实践指南》）：
#  1. 被测代码只取纯逻辑源文件（欧拉角→四元数、CSV 解帧），编成测试专用静态库
#     mgsdk_testlib，不拉入 UDP/socket 等系统依赖，可在无窗口环境毫秒级执行；
#  2. MSVC 下必须 gtest_force_shared_crt，使 GoogleTest 与主工程同为 /MD，
#     否则出现 CRT 运行时冲突（LNK4098 / 堆交叉释放）；
#  3. DISCOVERY_MODE PRE_TEST：用例清单在 ctest 运行期才向测试 exe 询问，
#     构建阶段不需要任何运行期依赖在 PATH 上；
#  4. SDK 本体保持 C++11，仅测试目录提升到 C++14（GoogleTest v1.14 的最低要求）。

set(MGSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../MotionGloveSDK")

# ---- GoogleTest 接入 -------------------------------------------------------
include(FetchContent)
option(USE_SYSTEM_GTEST "Use system installed GoogleTest instead of FetchContent" OFF)
set(GTEST_REPO "https://github.com/google/googletest.git" CACHE STRING "GoogleTest repository")
set(GTEST_TAG  "v1.14.0" CACHE STRING "GoogleTest tag")

if(USE_SYSTEM_GTEST)
    find_package(GTest 1.11 REQUIRED)
    set(MGSDK_GTEST_TARGETS GTest::gtest GTest::gtest_main)
else()
    set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)   # MSVC：与主工程同为 /MD
    set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)   # 不生成 install 规则
    set(BUILD_GMOCK            OFF CACHE BOOL "" FORCE)   # 只用 gtest，省编译时间
    FetchContent_Declare(googletest
        GIT_REPOSITORY "${GTEST_REPO}"
        GIT_TAG        "${GTEST_TAG}"
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(googletest)
    set(MGSDK_GTEST_TARGETS gtest gtest_main)
endif()

# GoogleTest v1.14 需要 C++14（只影响本目录创建的目标）
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 被测纯逻辑（测试专用静态库） ------------------------------------------
add_library(mgsdk_testlib STATIC
    "${MGSDK_DIR}/src/eulerToQuat.cpp"
    "${MGSDK_DIR}/src/decodeAsGloveCSV.cpp"
)
target_include_directories(mgsdk_testlib PUBLIC "${MGSDK_DIR}")
if(MSVC)
    target_compile_options(mgsdk_testlib PRIVATE /utf-8 /MP /D_CRT_SECURE_NO_WARNINGS)
endif()

# ---- 测试可执行文件 --------------------------------------------------------
add_executable(mgsdk_unit_tests
    eulerToQuat_test.cpp
    decodeAsGloveCSV_test.cpp
    readWriteLock_test.cpp
)
target_include_directories(mgsdk_unit_tests PRIVATE "${MGSDK_DIR}")
target_link_libraries(mgsdk_unit_tests PRIVATE mgsdk_testlib ${MGSDK_GTEST_TARGETS})
if(MSVC)
    target_compile_options(mgsdk_unit_tests PRIVATE /utf-8 /MP)
endif()

# ---- ctest 注册 ------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(mgsdk_unit_tests
    TEST_PREFIX "Unit."
    DISCOVERY_MODE PRE_TEST)
