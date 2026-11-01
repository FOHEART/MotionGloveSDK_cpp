# 多格式数据流解码（32 / 42 / OpenXR）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让接收端在解析前自动判定本帧是 32 段、42 段还是 OpenXR 布局，按对应布局与列数校验，并通过新增的格式查询接口与三个按格式取帧接口交付数据。

**Architecture:** 判定逻辑全部下沉到 `decodeAsGloveCSV_Func`（唯一被单测覆盖的解码入口）；帧头解析后按 `fn` 之后的标记决定期望列数，无标记时按列数反推 32/42。解码结果始终写入容量 42 的 `SingleSkeletonDef` 数组，由接收线程依据 `StreamHeader::skeletonFormat` 拷进 32 或 42 容器。公开接口层只做「查格式 → 按格式取帧」的分发。

**Tech Stack:** C++11 共享库（MSVC / GCC）、CMake、GoogleTest v1.14（仅测试目录 C++14）、Doxygen 1.11。

**Spec:** [docs/superpowers/specs/2026-11-01-multiformat-stream-decode-design.md](../specs/2026-11-01-multiformat-stream-decode-design.md)

## Global Constraints

- C++11；SDK 本体不得使用 C++14 及以上特性。
- `sizeof(StreamHeader)` 必须保持不变：新增 `skeletonFormat` 占 1 字节，`resv[40]` 同步改为 `resv[39]`。
- Doxygen `WARN_AS_ERROR=YES`：新增的文件、枚举值、结构体成员、函数必须有 `@brief`；函数需 `@param` / `@return`；`@cond` 后不得跟说明文字；单个文件只写一个 `@file`；`#名称` 后不紧跟中文句号。
- 测试必须全绿：`tools\build_and_test.bat Debug` 结束时输出 `[OK] all tests passed`。
- 既有 19 个用例不得删改断言（可新增）。
- 每个用例必须使用互不相同的帧号（进程级静态分帧管理器，见 `tests/README.md`）。
- 提交时只 `git add` 本任务列出的文件；仓库内存在大量与本任务无关的既有改动，不得一并提交。

## Review Focus

1. **演员名恰好等于 `skeleton_42`、且帧内无标记** — 期望：按列数判定，不得被误认成标记（Task 2 的 `ActorNameResemblingMarkerIsNotTreatedAsMarker`）。
2. **标记与列数矛盾**（`skeleton_32` 却给 42 组列） — 期望：整帧丢弃返回 `-1`，不静默按列数改写格式（Task 2 的 `RejectsMarkerContradictingColumnCount`）。
3. **同一帧的不同子包标记不一致** — 期望：整帧丢弃，不把两段数据拼成错位帧（Task 2 的 `RejectsInconsistentMarkerAcrossSubpackages`）。
4. **调用方容量 32、来的却是 42 段帧** — 期望：返回 `-1`，不越界写（Task 2 的 `RejectsWhenCapacityBelowDetectedCount`）。
5. **未知标记 `skeleton_53`** — 期望：整帧丢弃返回 `-1`，绝不回退成 32 段继续解析（Task 2 的 `RejectsUnknownSkeletonMarker`）。

---

## 文件结构

| 文件 | 职责 | 动作 |
|---|---|---|
| `MotionGloveSDK/include/motionGloveSDK_commonDef.h` | 格式枚举 + `StreamHeader::skeletonFormat` | 修改 |
| `MotionGloveSDK/include/motionGloveSDK_skeletonOpenXRDef.h` | OpenXR 占位枚举与数量 | 新建 |
| `MotionGloveSDK/include/motionGloveSDKDef.h` | 32/OpenXR 容器、`gloveSkeletonIndex32`、管理器按格式存储 | 修改 |
| `MotionGloveSDK/include/motionGloveSDK.h` | 4 个公开接口声明 | 修改 |
| `MotionGloveSDK/inc/decodeAsGloveCSV.h` | 解码契约（容量语义、返回码） | 修改 |
| `MotionGloveSDK/src/decodeAsGloveCSV.cpp` | 格式判定、列数消歧、32 段落位 | 修改 |
| `MotionGloveSDK/src/motionGloveSDK.cpp` | 接收线程、管理器写入、接口实现 | 修改 |
| `MotionGloveSDK/inc/eulerToQuat.h` | 注释中的旧函数名 | 修改 |
| `MotionGloveSDK/CMakeLists.txt` | 头文件清单 | 修改 |
| `MotionGloveSDK_examples/src/motionGloveSDK_example2/3/4.cpp` | 取帧函数改名 | 修改 |
| `MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp` | 改用 32 版接口 | 修改 |
| `MotionGloveSDK_examples/src/motionGloveSDK_example1.cpp` | 注释里的函数名 | 修改 |
| `tests/unit/decodeAsGloveCSV_test.cpp` | 12 个新用例 | 修改 |
| `tests/README.md` | 用例数从 19 更新 | 修改 |

---

### Task 1: 格式枚举与输出容器（类型层）

**Files:**
- Modify: `MotionGloveSDK/include/motionGloveSDK_commonDef.h`（`MVSDK_ChannelOrder_StringDesc` 之后新增枚举；`_stream_header` 的 `rightHandGesture` 与 `resv` 之间插入字段）
- Create: `MotionGloveSDK/include/motionGloveSDK_skeletonOpenXRDef.h`
- Modify: `MotionGloveSDK/include/motionGloveSDKDef.h`
- Modify: `MotionGloveSDK/CMakeLists.txt:29-38`（`HEADER_FILES` 增加新头文件）

**Interfaces:**
- Produces（后续任务依赖的确切名字）：
  - `typedef enum _GloveStreamSkeletonFormat : uint8_t { GloveStreamFormat_Skeleton32 = 0, GloveStreamFormat_Skeleton42 = 1, GloveStreamFormat_OpenXR = 2, GloveStreamFormat_Count } GloveStreamSkeletonFormat_TypeDef;`
  - `StreamHeader::skeletonFormat`（类型 `GloveStreamSkeletonFormat_TypeDef`）
  - `KHHSOpenXR_SkeletonCount`（值 52）、`kinemHumanHandsSkeletonOpenXRIndex`
  - `KHHS32PosAttitude_TypeDef`、`KHHSOpenXRPosAttitude_TypeDef`
  - `SingleSkeletonDef` 的 union 成员 `gloveSkeletonIndex32`（类型 `kinemHumanHandsSkeleton32Index`）
  - `GloveSuitManager_TypeDef` 的成员：`frameFormat`、`posAttitude32`、`posAttitude42`（**移除**旧成员 `posAttitude`）

- [ ] **Step 1: 在 `motionGloveSDK_commonDef.h` 加入格式枚举**

放在 `MVSDK_ChannelOrder_StringDesc` 数组之后、`_stream_header` 定义之前。每个枚举值都要有 `///<` 说明。

- [ ] **Step 2: 改 `_stream_header`：插入格式字段并收缩保留区**

在 `rightHandGesture` 之后、`resv` 之前插入：

```c
	GloveStreamSkeletonFormat_TypeDef skeletonFormat;  ///< 本帧骨骼段数格式；帧内无标记时为 GloveStreamFormat_Skeleton32
```

并把 `unsigned char resv[40];` 改为 `unsigned char resv[39];`。

同时订正两条过时注释（值不变，仅说明）：
`leftFigureSkeletonCount` → `左手指骨的骨骼数量：32 段格式为 15，42 段格式为 20`；`rightFigureSkeletonCount` 同理。

- [ ] **Step 3: 新建 `motionGloveSDK_skeletonOpenXRDef.h`**

必须包含文件级 `@file` / `@brief` 注释块（参照 `motionGloveSDK_skeleton32Def.h` 的头部写法）。内容只有两项：

```c
typedef enum : uint32_t
{
	KHHSOpenXR_SkeletonIndexBegin = 0,  ///< 关节索引起点（布局未定义，仅占位）
	KHHSOpenXR_SkeletonCount      = 52  ///< 关节数量：规划每手 26 个、双手 52；布局确定前可能调整
} kinemHumanHandsSkeletonOpenXRIndex;
```

- [ ] **Step 4: 在 `motionGloveSDKDef.h` 扩展类型**

1) `#include "motionGloveSDK_skeletonOpenXRDef.h"`（跟在另外两个 skeleton 头文件之后）。
2) `_SingleSkeleton` 的匿名 union 内新增一行：
   `kinemHumanHandsSkeleton32Index gloveSkeletonIndex32;  ///< 本段骨骼的编号（32 段索引）`
3) 在 `KHHS42PosAttitude_TypeDef` 之后新增两个同构容器（字段顺序与 42 版保持一致：hdr → skeletons → remoteIP → remotePort → sdkVersion）：
   `KHHS32PosAttitude_TypeDef`（`skeletons[KHHS32_SkeletonCount]`）与
   `KHHSOpenXRPosAttitude_TypeDef`（`skeletons[KHHSOpenXR_SkeletonCount]`）。
4) `GloveSuitManager_TypeDef`：删除 `posAttitude`，改为

```c
	GloveStreamSkeletonFormat_TypeDef frameFormat;  ///< 缓存帧的骨骼格式
	KHHS32PosAttitude_TypeDef posAttitude32;  ///< frameFormat 为 GloveStreamFormat_Skeleton32 时有效
	KHHS42PosAttitude_TypeDef posAttitude42;  ///< frameFormat 为 GloveStreamFormat_Skeleton42 时有效
```

> 用两个并列成员而不是 union：`SingleSkeletonDef` 有用户提供的构造函数，C++11 下含此类成员的 union 其默认构造函数会被删除，导致全局数组无法构造。代价是每槽多约 6.4 KB（32 槽合计约 205 KB）。

- [ ] **Step 5: 更新 `MotionGloveSDK/CMakeLists.txt` 的 `HEADER_FILES`**

在 `"include/motionGloveSDK_skeleton42Def.h"` 之后加 `"include/motionGloveSDK_skeletonOpenXRDef.h"`。

- [ ] **Step 6: 让编译先通过（中间态，不单独提交）**

`motionGloveSDK.cpp` 仍在引用已删除的 `posAttitude`。把其中三处 `gloveSuitManager[index].posAttitude` 就地改名为 `posAttitude42`（行为不变，Task 3 再补格式分支）。

- [ ] **Step 7: 构建并跑既有测试，确认无回归**

Run: `tools\build_and_test.bat Debug`
Expected: 结尾 `[OK] all tests passed`，`ctest` 报 19/19。

- [ ] **Step 8: 提交**

```bash
git add MotionGloveSDK/include/motionGloveSDK_commonDef.h MotionGloveSDK/include/motionGloveSDK_skeletonOpenXRDef.h MotionGloveSDK/include/motionGloveSDKDef.h MotionGloveSDK/CMakeLists.txt MotionGloveSDK/src/motionGloveSDK.cpp
git commit -m "feat(sdk): add skeleton format enum and 32/OpenXR output containers"
```

---

### Task 2: 解码器多格式判定与 32 段落位

**Files:**
- Modify: `MotionGloveSDK/inc/decodeAsGloveCSV.h`（契约与返回码）
- Modify: `MotionGloveSDK/src/decodeAsGloveCSV.cpp`（`GloveSplitFrameMgr`、`parseFrame()`、`decodeAsGloveCSV_Func`）
- Test: `tests/unit/decodeAsGloveCSV_test.cpp`

**Interfaces:**
- Consumes: Task 1 的 `GloveStreamSkeletonFormat_TypeDef`、`StreamHeader::skeletonFormat`、`gloveSkeletonIndex32`。
- Produces:
  - `#define MGSDK_DECODE_OPENXR_UNSUPPORTED (-2)`（`inc/decodeAsGloveCSV.h`）
  - `decodeAsGloveCSV_Func(..., unsigned int skeletonCount)` 的 `skeletonCount` 语义改为「输出数组容量」，允许 32 或 42。

- [ ] **Step 1: 先在 `inc/decodeAsGloveCSV.h` 写明新契约**

新增宏 `MGSDK_DECODE_OPENXR_UNSUPPORTED (-2)`；把 `@param[in] skeletonCount` 的说明改为「输出数组容量，必须为 32 或 42；检测到的段数超过该值返回 -1」；`@return` 增加 `-2` 一行：「帧头声明 `skeleton_openxr`，布局未定义，整帧丢弃，此时 `sh->skeletonFormat` 已被置为 `GloveStreamFormat_OpenXR`」。

- [ ] **Step 2: 写失败用例 —— 共 12 个**

在 `tests/unit/decodeAsGloveCSV_test.cpp` 的匿名 namespace 内，给 `makeEulerValues()` 增加 `unsigned int count` 参数（现有调用处传 `kSkeletonCount`），并新增带标记的帧头助手：

```cpp
/// 构造带骨骼段数标记的帧头；marker 为空串时不输出标记。
static std::string makeHeaderWithMarker(unsigned int frameNumber, int subIndex, int subTotal,
                                        const char* attitudeToken, const char* marker);
```

用例与断言（帧号统一从 600 起，避开既有用例已占用的 100/110/120/130/200/300/400/500）：

| 用例名 | 输入 | 断言 |
|---|---|---|
| `ParsesExplicitSkeleton42Marker` | 标记 `skeleton_42`，42×6 数值，fn 600 | `ret == 0`；`sh.skeletonFormat == GloveStreamFormat_Skeleton42`；`sh.leftFigureSkeletonCount == 20`；`ssd[4].skeletonName == "RightHandThumb3End"` |
| `ParsesUnmarked32Frame` | 无标记，32×6 数值，fn 601 | `ret == 0`；`skeletonFormat == GloveStreamFormat_Skeleton32`；`leftFigureSkeletonCount == 15`；`ssd[4].skeletonName == "RightHandIndex1"` |
| `Unmarked42FrameStillTreatedAs42` | 无标记，42×6 数值，fn 602 | `ret == 0`；`skeletonFormat == GloveStreamFormat_Skeleton42`；`leftFigureSkeletonCount == 20` |
| `RejectsOpenXRMarker` | 标记 `skeleton_openxr`，fn 603 | `ret == MGSDK_DECODE_OPENXR_UNSUPPORTED`；`sh.skeletonFormat == GloveStreamFormat_OpenXR` |
| `RejectsUnknownSkeletonMarker` | 标记 `skeleton_53`，fn 604 | `ret == -1` |
| `RejectsMarkerContradictingColumnCount` | 标记 `skeleton_32`，42×6 数值，fn 605 | `ret == -1` |
| `RejectsWhenCapacityBelowDetectedCount` | 标记 `skeleton_42`，42×6 数值，fn 606，容量传 32 | `ret == -1` |
| `RejectsUnmarkedCountMatchingNeither` | 无标记，30×6 数值，fn 607 | `ret == -1` |
| `RejectsInconsistentMarkerAcrossSubpackages` | 同 fn 608 两子包：`1/2` 带 `skeleton_32`、`2/2` 带 `skeleton_42`，各 16×6 | `ret == -1` |
| `Skeleton32FillsOnlyThirtyTwoSkeletons` | 标记 `skeleton_32`，32×6，fn 609；`ssd` 预先全零 | `ret == 0`；`ssd[31].skeletonName == "RightHandPinky3"`；`ssd[32].skeletonName[0] == '\0'` |
| `Skeleton32Uses32IndexTable` | 同上输入，fn 610 | `ssd[15].gloveSkeletonIndex32 == KHHS32_RightHandPinky3`；`ssd[0].gloveSkeletonIndex32 == KHHS32_RightHand` |
| `ActorNameResemblingMarkerIsNotTreatedAsMarker` | 演员名 `skeleton_42`、**无标记**、32×6，fn 611 | `ret == 0`；`skeletonFormat == GloveStreamFormat_Skeleton32` |

- [ ] **Step 3: 运行测试确认失败**

Run: `tools\build_and_test.bat Debug Unit.DecodeAsGloveCSV.*`
Expected: 12 个新用例全部 FAIL。

- [ ] **Step 4: `GloveSplitFrameMgr` 增加格式状态**

新增成员 `GloveStreamSkeletonFormat_TypeDef markerFormat;`、`bool hasMarker;`、`unsigned int detectedSkeletonCount;`，在 `reset()` 中分别复位为 `GloveStreamFormat_Skeleton32` / `false` / `0`。

- [ ] **Step 5: 在 `decodeAsGloveCSV_Func` 里定位并解释标记**

在解析 `fn` 的代码之后、`subpackage` 分支之前（`fnDis` 是帧号所在下标）：

```cpp
// 帧号之后若还有 token，则可能是骨骼段数标记
if (fnDis + 1 < (int)splitHeaderVec.size())
{
    const std::string& t = splitHeaderVec[fnDis + 1];
    if (t == "skeleton_32")      { hasMarker = true; markerFormat = GloveStreamFormat_Skeleton32; }
    else if (t == "skeleton_42") { hasMarker = true; markerFormat = GloveStreamFormat_Skeleton42; }
    else if (t == "skeleton_openxr")
    {
        sh->skeletonFormat = GloveStreamFormat_OpenXR;
        return MGSDK_DECODE_OPENXR_UNSUPPORTED;   // 必须在触碰 m_GloveSplitFrameMgr 之前返回
    }
    else if (t.compare(0, 9, "skeleton_") == 0)
    {
        return -1;                                 // 未知标记，整帧丢弃，不回退成 32
    }
}
```

两点注意：`fn` 未出现时 `fnDis` 视为 -1，不进入该分支；`skeleton_openxr` 的返回必须早于任何管理器状态改写，避免污染下一帧。

- [ ] **Step 6: 子包累积时校验标记一致性**

`new frame` 分支把 `hasMarker` / `markerFormat` 写入管理器；`add to current frame` 分支若 `hasMarker != m_GloveSplitFrameMgr.hasMarker` 或 `markerFormat != m_GloveSplitFrameMgr.markerFormat`，则 `m_GloveSplitFrameMgr.reset(); return -1;`。

- [ ] **Step 7: `parseFrame()` 按格式定列数**

把现有的 `if (dataVec.size() == groupItemCount*KHHS42_SkeletonCount)` 换成下面这段判定，并把解析循环上界从 `KHHS42_SkeletonCount` 改为 `detected`：

```cpp
const size_t n = dataVec.size();
unsigned int detected = 0;
if (m_GloveSplitFrameMgr.hasMarker)
{
    if (markerFormat == GloveStreamFormat_Skeleton42 && n == groupItemCount * 42) detected = 42;
    else if (markerFormat == GloveStreamFormat_Skeleton32 && n == groupItemCount * 32) detected = 32;
}
else
{
    if (n == groupItemCount * 42) detected = 42;        // 现网 42 段帧无标记，靠列数识别
    else if (n == groupItemCount * 32) detected = 32;
}
if (0 == detected) { printf("[err]"); return false; }
```

解析成功后把 `detected` 写入 `m_GloveSplitFrameMgr.detectedSkeletonCount`。

- [ ] **Step 8: `decodeAsGloveCSV_Func` 出口按段数落位**

1) 入口容量校验：`if (skeletonCount != KHHS32_SkeletonCount && skeletonCount != KHHS42_SkeletonCount) return -1;`（替换现有的 `if (KHHS42_SkeletonCount != skeletonCount) return -1;`）。
2) 把 `sh->leftFigureSkeletonCount` / `rightFigureSkeletonCount` 的赋值从帧头解析处**移到**解析成功之后，按 `detected` 取 `15`（32 段）或 `20`（42 段）；`bodySkeletonCount` 恒为 2。
3) `parseFrame()` 成功后，若 `m_GloveSplitFrameMgr.detectedSkeletonCount > skeletonCount` 则 `return -1`。
4) 设置 `sh->skeletonFormat`（32 → `GloveStreamFormat_Skeleton32`，42 → `GloveStreamFormat_Skeleton42`）。
5) 落位循环上界改为 `detected`，并分表填写：

```cpp
if (42 == detected)
{
    ssd[i].gloveSkeletonIndex = (kinemHumanHandsSkeleton42Index)i;
    kinemHumanHandsSkeleton42[i].copy(ssd[i].skeletonName, kinemHumanHandsSkeleton42[i].length(), 0);
}
else
{
    ssd[i].gloveSkeletonIndex32 = (kinemHumanHandsSkeleton32Index)i;
    kinemHumanHandsSkeleton32[i].copy(ssd[i].skeletonName, kinemHumanHandsSkeleton32[i].length(), 0);
}
```

位置 / 欧拉角 / 四元数的赋值保持原样（内部数组容量 42，32 段只用到前 32 项）。

- [ ] **Step 9: 运行测试确认通过**

Run: `tools\build_and_test.bat Debug Unit.DecodeAsGloveCSV.*`
Expected: 22 个用例全 PASS（既有 10 + 新增 12）。

- [ ] **Step 10: 提交**

```bash
git add MotionGloveSDK/inc/decodeAsGloveCSV.h MotionGloveSDK/src/decodeAsGloveCSV.cpp tests/unit/decodeAsGloveCSV_test.cpp
git commit -m "feat(sdk): auto-detect skeleton_32/42/openxr frame layout in decoder"
```

---

### Task 3: 公开接口层（改名 + 三个新接口 + 接收线程）

**Files:**
- Modify: `MotionGloveSDK/include/motionGloveSDK.h:99`（声明区）
- Modify: `MotionGloveSDK/src/motionGloveSDK.cpp`（接收线程 198-250、`resetKHHS42PosAttitude` 附近、接口实现 701-735）
- Modify: `MotionGloveSDK_examples/src/motionGloveSDK_example2.cpp:77`、`example3.cpp:80`、`example4.cpp:80`、`example_vtk.cpp:177`、`example1.cpp:8`
- Modify: `MotionGloveSDK/inc/eulerToQuat.h:8`

**Interfaces:**
- Consumes: Task 1 的容器与 `frameFormat`；Task 2 的 `MGSDK_DECODE_OPENXR_UNSUPPORTED` 与 `sh->skeletonFormat`。
- Produces（对外 ABI）：
  - `int MotionGloveSDK_GetSkeletonsFrameType(const char* actorName, GloveStreamSkeletonFormat_TypeDef* pFormat);`
  - `int MotionGloveSDK_GetSkeletons42Frame(const char* actorName, KHHS42PosAttitude_TypeDef* pKHHS42PosAttitude);`
  - `int MotionGloveSDK_GetSkeletons32Frame(const char* actorName, KHHS32PosAttitude_TypeDef* pKHHS32PosAttitude);`
  - `int MotionGloveSDK_GetSkeletonsOpenXRFrame(const char* actorName, KHHSOpenXRPosAttitude_TypeDef* pKHHSOpenXRPosAttitude);`

- [ ] **Step 1: 在 `motionGloveSDK.h` 写四个声明**

删除旧声明 `MotionGloveSDK_GetGloveSkeletonsFrame`，按 `Interfaces` 的四条签名新增；每条都要完整的 `@brief`（中英双行，沿用文件既有风格）、`@param`、`@return`。必须写进注释的语义：

- `GetSkeletonsFrameType`：返回当前缓存帧的格式，`0` 成功、`-1` 流不存在或参数非法。
- `GetSkeletons42Frame` / `GetSkeletons32Frame`：流存在但格式不符时返回 `-1`。
- `GetSkeletonsOpenXRFrame`：布局未定义、当前收不到该格式，**恒返回 `-1`**。

同时更新文件头「约定」段：数据输出结构有两种，按 `GetSkeletonsFrameType` 的返回值选择。

- [ ] **Step 2: `motionGloveSDK.cpp` 增加 32 容器复位助手**

紧跟 `resetKHHS42PosAttitude` 之后新增 `static void resetKHHS32PosAttitude(KHHS32PosAttitude_TypeDef* input)`，实现与 42 版一致（`memset` 整个结构）。

- [ ] **Step 3: 接收线程按格式写管理器**

把 `memcpy(&gloveSuitManager[index].posAttitude42, &kpa, ...)` 那段换成按 `kpa.hdr.skeletonFormat` 分派：32 段时 `resetKHHS32PosAttitude(&slot->posAttitude32)`，拷 `hdr`、前 `KHHS32_SkeletonCount` 个 `SingleSkeletonDef`、`remoteIP[0..3]`、`remotePort`、`sdkVersion`；42 段时整体 `memcpy` 到 `posAttitude42`。两个分支都写 `slot->frameFormat = kpa.hdr.skeletonFormat;` 后再置 `isNewFramePending = 1`。

- [ ] **Step 4: 接收线程处理 `-2`**

`decodeRet` 判定改为：等于 `MGSDK_DECODE_OPENXR_UNSUPPORTED` 时累加一个 `static unsigned int` 计数，仅在第 1 次与每 100 次打印一条告警（含累计次数），然后 `continue`；等于 `-1` 时保持原样 `continue`。

- [ ] **Step 5: 实现四个接口**

抽一个文件内静态助手 `static int findActorIndex(const char* actorName)`，把现有 `GetGloveSkeletonsFrame` 里「逐字节量长度 + `GloveSuitManager_SearchByName`」那段提取进来（`actorName` 为空或未找到返回 `-1`）。四个接口都基于它：

- `GetSkeletonsFrameType`：读锁下取 `slot->frameFormat` 写入 `*pFormat`。
- `GetSkeletons42Frame` / `GetSkeletons32Frame`：读锁下先比对 `slot->frameFormat`，不符 `return -1`，相符才 `memcpy`。
- `GetSkeletonsOpenXRFrame`：参数校验后直接 `return -1`。

- [ ] **Step 6: 同步 6 处调用点与注释**

`example2/3/4.cpp` 的 `MotionGloveSDK_GetGloveSkeletonsFrame` → `MotionGloveSDK_GetSkeletons42Frame`；
`example_vtk.cpp:177` → `MotionGloveSDK_GetSkeletons32Frame`；
`example1.cpp:8` 与 `eulerToQuat.h:8` 的注释文字同步改名。

- [ ] **Step 7: 构建全部目标并跑测试**

Run: `tools\build_and_test.bat Debug`
Expected: 结尾 `[OK] all tests passed`，示例工程一并编译通过。

再执行 `rg -n "GetGloveSkeletonsFrame" MotionGloveSDK MotionGloveSDK_examples` 确认无命中（`openspec/` 下的历史文档不计）。

- [ ] **Step 8: 提交**

```bash
git add MotionGloveSDK/include/motionGloveSDK.h MotionGloveSDK/src/motionGloveSDK.cpp MotionGloveSDK/inc/eulerToQuat.h MotionGloveSDK_examples/src/motionGloveSDK_example1.cpp MotionGloveSDK_examples/src/motionGloveSDK_example2.cpp MotionGloveSDK_examples/src/motionGloveSDK_example3.cpp MotionGloveSDK_examples/src/motionGloveSDK_example4.cpp MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp
git commit -m "feat(sdk)!: rename 42 frame getter and add 32/OpenXR/frame-type APIs"
```

---

### Task 4: 文档同步与最终验收

**Files:**
- Modify: `tests/README.md`（用例总数与表格）
- Modify: `docs/superpowers/specs/2026-11-01-multiformat-stream-decode-design.md`（§5.8 与实现差异回写）

**Interfaces:**
- Consumes: Task 1–3 的全部产出。
- Produces: 无代码接口。

- [ ] **Step 1: 更新 `tests/README.md`**

「2. 测试用例总览」的标题与表格改为 31 个用例（既有 19 + 新增 12），并在 `decodeAsGloveCSV_test.cpp` 那一行补上新增覆盖点：格式标记识别、32/42 列数消歧、OpenXR 与未知标记丢弃、子包标记一致性、容量校验。

- [ ] **Step 2: 把实现差异回写设计文档**

在 §5.8 补一句：因 C++11 下含非平凡成员的 union 默认构造被删除，改用两个并列成员（`posAttitude32` / `posAttitude42`），每槽多约 6.4 KB。
若 Task 3 的实际返回码或命名与 §5.6/§5.7 有出入，一并在文档里订正为实现的真实情况。

- [ ] **Step 3: 文档零告警验收**

Run: `gen_docs.bat`
Expected: 退出码 0，且 `doxygen_warnings.log` 为 0 字节，`docs\html\index.html` 重新生成。

- [ ] **Step 4: 全量测试验收**

Run: `tools\build_and_test.bat Debug`
Expected: `ctest` 报 `100% tests passed`，用例总数 31。

- [ ] **Step 5: 提交**

```bash
git add tests/README.md docs/superpowers/specs/2026-11-01-multiformat-stream-decode-design.md
git commit -m "docs: update test overview and design notes for multi-format decoding"
```

---

## 自检记录

- **规格覆盖**：设计 §5.1→Task 1 Step 1；§5.2→Task 1 Step 2；§5.3→Task 2 Step 5；§5.4→Task 2 Step 7；§5.5→Task 2 Step 4/6/8；§5.6→Task 2 Step 1/4；§5.7→Task 3 Step 1/5/6；§5.9→Task 1 Step 3/4、Task 3 Step 5。§7 的四项「不做」未派生任务，符合非目标。
- **Review Focus 五项**均已在 Task 2 Step 2 的用例表里落到具体用例名。
- **类型一致性**：`gloveSkeletonIndex32`、`posAttitude32`、`posAttitude42`、`frameFormat`、`MGSDK_DECODE_OPENXR_UNSUPPORTED`、`kinemHumanHandsSkeletonOpenXRIndex` 在 Task 1/2/3 中同名同型。
