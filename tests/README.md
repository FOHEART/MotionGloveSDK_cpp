## 单元测试（TDD 实践记录）

本文档记录 MotionGloveSDK 单元测试的接入方式、红绿循环过程与验收结果，
遵循 `D:\autotools\skills\TDD与GoogleTest实践指南.md` 与
`D:\autotools\skills\代码注释规范_Doxygen.md` 两份工程指南。

## 1. 框架接入（GoogleTest + CMake FetchContent）

- 顶层 `CMakeLists.txt` 末尾追加 `option(BUILD_TESTS ... ON)`，
  开启后 `add_subdirectory(tests/unit)` 并 `enable_testing()`。
- `tests/unit/CMakeLists.txt`：
  - `FetchContent` 拉取 GoogleTest **v1.14.0**（GoogleTest v1.14 起要求 C++14，
    SDK 本体保持 C++11，仅测试目录内 `set(CMAKE_CXX_STANDARD 14)`）。
  - `gtest_force_shared_crt ON`（配合 MSVC 动态运行时）、`INSTALL_GTEST`/`BUILD_GMOCK OFF`。
  - `gtest_discover_tests(... TEST_PREFIX "Unit." DISCOVERY_MODE PRE_TEST)`
    使每个用例注册为独立 CTest，测试名形如 `Unit.EulerToQuat.*`。
- 被测代码策略：只把**纯逻辑源文件**（`eulerToQuat.cpp`、`decodeAsGloveCSV.cpp`）
  编成语料静态库 `mgsdk_testlib`，避免对未导出符号的 DLL 产生 unresolved externals。
- 离线环境：默认走 FetchContent 下载；如网络受限可预先解压 GoogleTest 源码，
  配置时传 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<路径>`。

## 2. 测试用例总览（31 个）

| 测试文件 | 用例数 | 覆盖点 |
|---|---|---|
| `eulerToQuat_test.cpp` | 7 | 6 种旋转顺序解析解、零角度单位四元数、四元数模长=1、非有限输入拒绝、越界顺序、空指针 |
| `decodeAsGloveCSV_test.cpp` | 22 | 空参数拒绝、容量校验、欧拉帧/四元数帧解析、子包不完整、双子包拼接、字段数错误、缺字段；**格式判定**：`skeleton_42` 标记、无标记 32 段按列数识别、无标记 42 段兼容、`skeleton_openxr` 与未知标记丢弃、标记与列数矛盾丢弃、子包间标记不一致丢弃、32 段只落前 32 项且用 32 段名表、演员名与标记同形不误判 |
| `readWriteLock_test.cpp` | 2 | 双读者并发共享、写者互斥阻塞（future 限时防挂死） |

## 3. 红 → 绿 循环记录

第一轮（红）：19 个用例中 **3 个失败**（预期红，非测试缺陷）：

| 用例 | 红的原因 | 对应修复（绿） |
|---|---|---|
| `RejectsNonFiniteInputKeepsOutput` | NaN/Inf 未拦截，污染输出四元数 | 入口处 `std::isfinite` 三项检查，非有限输入直接 return 保持原值 |
| `InvalidChannelOrderYieldsIdentity` | 越界枚举走未初始化分支，读到栈垃圾（-0.666/0.333） | 局部矩阵初始化为单位矩阵，default 分支输出单位四元数 |
| `NullOutputPointerDoesNotCrash` | 空指针解引用，SEH 0xc0000005 崩溃 | 入口处 `if (fData == nullptr) return;` |

修复后复跑：**19/19 全绿**，总计约 0.7 秒（含两个读写锁时序用例）。

## 4. 验收结果（三查）

| 验收项 | 结果 |
|---|---|
| `ctest --test-dir out/build/tdd-nmake` | **100% passed, 19/19** |
| `doxygen Doxyfile` 退出码 | **exit = 0**（WARN_AS_ERROR=YES） |
| `doxygen_warnings.log` | **0 字节**（零告警） |
| HTML 产出 | `docs/html/index.html`（约 2.9 MB，189 个文件） |

## 5. 注意事项（踩坑记录）

- **浮点断言**：禁止 `EXPECT_EQ` 比较浮点，统一使用自定义宏
  `EXPECT_NEAR_FLOAT(actual, expected, eps)`（见 `gtest_assertions.h`）。
  自定义宏而非 `EXPECT_PRED_FORMAT3`，规避旧标准下的参数个数编译错误。
- **TEST 宏与 Doxygen**：测试用例注释必须用 `\@test/\@brief/\@details/\@expected/\@priority`
  五字段模板，且 TEST 宏整体用 `/// \@cond DOC_HIDDEN` … `/// \@endcond` 包裹，
  避免被测头文件在测试翻译单元中被 Doxygen 重复扫描。
- **cond 标签（DOC_HIDDEN）后不能直接跟说明文字**：cond 标签后面写 "TEST 宏展开体…" 这类文字
  会被当作条件表达式求值报 "problem evaluating expression"，条件标签后只能跟名字。
- **静态状态串扰**：`decodeAsGloveCSV` 内部是进程级静态分帧管理器，
  用例必须使用互不相同的帧号，避免跨用例的状态残留。
- **线程用例防挂死**：用 `std::async` + `future.wait_for` 限时断言，
  缺陷场景最多 2 秒内失败，而不是挂死测试进程。