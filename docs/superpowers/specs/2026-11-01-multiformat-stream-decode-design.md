# 多格式数据流解码（32 / 42 / OpenXR）设计

日期：2026-11-01
相关规格：[MOTIONGLOVE_SDK_STREAM_FORMAT.md](../../../MotionGloveSDK/include/MOTIONGLOVE_SDK_STREAM_FORMAT.md)（§2.9、§3、§6）

## 1. 意图与理由

发送端会在帧头 `fn <帧号>` 之后新增一个可缺省的骨骼段数标记：
`skeleton_32` / `skeleton_42` / `skeleton_openxr`。接收端必须自己判断本帧是哪种布局再解析，
而不是像现在这样无条件按 42 段处理。

当前接收链路把 42 硬编码在三处：接收线程传 `KHHS42_SkeletonCount`；
`decodeAsGloveCSV_Func` 入口 `if (KHHS42_SkeletonCount != skeletonCount) return -1;`；
`parseFrame()` 按 `fieldCount × 42` 校验列数。输出结构也只有 `KHHS42PosAttitude_TypeDef`。

标记一旦上线，现有实现就会失效：32 段帧的列数（192 / 224 / 96 …）会被 42 段的列数校验直接丢弃。

## 2. 目标 / 非目标

**目标**

1. 解析前自动判定本帧格式（32 / 42 / OpenXR），按对应布局与列数校验。
2. 32 段帧有独立、语义正确的输出类型与取帧接口（方案 C）。
3. `skeleton_openxr` 与未知 `skeleton_*` 一律整帧丢弃，绝不复用 32/42 布局猜测。
4. 现有 42 段调用方行为不变（取帧函数改名为 `MotionGloveSDK_GetSkeletons42Frame`）。
5. 新增「当前缓存帧是什么格式」查询接口，调用方据此选择取帧函数。

**非目标**

- OpenXR 的实际布局解析（布局未定义，本期只保留枚举值与丢弃路径）。
- 发送端改造与 JSON 配置项（属发送端 SDK）。
- CSV 录制侧的表头重排（文档 §8 标注「尚未实现」，本期不动）。

**反目标**（技术上跑通但算失败）

- 把 32 段帧塞进 42 容器、让调用方自己猜哪 10 个位置是垃圾数据。
- 用「末段位置补零 / 上采样」伪造 42 段——指尖末端位置在发送端由 `handJointPosition42` 提供，
  接收端无法从 32 段还原。
- 对 `skeleton_openxr` 回退成 32 段继续解析（会静默错位而非报错）。

## 3. 约束

- C++11，Windows/Linux 双平台，MSVC 与 GCC 都要过。
- `StreamHeader` 是 `pragma pack(1)` 的公开结构，直接 `memcpy` 到调用方缓冲区；
  改动必须保持 `sizeof(StreamHeader)` 不变。
- Doxygen `WARN_AS_ERROR=YES`：新增类型/成员/函数必须带完整注释，否则文档构建失败。
- 现有 19 个单测必须全绿，且不得削弱既有断言。

## 4. 方案（已选定 C）

| 方案 | 做法 | 结论 |
|---|---|---|
| A | 按 32 顺序直接填 42 容器，后 10 段留零值 | 否：调用方要自己判断有效性，语义含糊 |
| B | 32 上采样成 42，末端由父骨骼推导 | 否：末端位置无法还原，属伪造数据 |
| **C** | 新增 `KHHS32PosAttitude_TypeDef` + 独立取帧接口 | **选定**：32/42 各自原生，VTK 示例那个悬空类型也随之落地 |

## 5. 已确定的实现决策

### 5.1 格式枚举

`motionGloveSDK_commonDef.h` 新增：

```c
typedef enum _GloveStreamSkeletonFormat : uint8_t
{
    GloveStreamFormat_Skeleton32 = 0,  // 默认：帧内无标记即此值
    GloveStreamFormat_Skeleton42 = 1,
    GloveStreamFormat_OpenXR     = 2,  // 保留，布局未定义
    GloveStreamFormat_Count
} GloveStreamSkeletonFormat_TypeDef;
```

值 0 定为 32，使 `StreamHeader` 被 `memset` 清零后天然等于「老格式」，与 §2.9
「没有该字段即 `skeleton_32`」对齐。

### 5.2 `StreamHeader` 增加格式字段（尺寸不变）

在 `rightHandGesture` 之后、`resv` 之前插入 `skeletonFormat`（1 字节），
并把 `resv[40]` 改为 `resv[39]`。`pragma pack(1)` 下 `sizeof(StreamHeader)` 保持不变，
ABI 尺寸不受影响；新增字段落在既有保留区内，老调用方读到的仍是清零值。

### 5.3 检测规则

帧头按空格分词后：

1. 定位 `fn` 关键字，紧随其后的 token 是帧号；
2. 帧号之后的 token 若**精确等于** `skeleton_32` / `skeleton_42` / `skeleton_openxr`
   → 取对应格式；
3. 否则（直接是 `subpackage` 或帧头结束）→ 进入 5.4 的列数判定；
4. 帧号之后的 token 形如 `skeleton_` 前缀但不在三者之内 → 未知格式，**整帧丢弃**。

### 5.4 无标记帧的歧义消解（本设计的关键取舍）

文档规定「无标记 = 32 段」，但现网 42 段帧同样没有标记。若严格按文档执行，
今天能正常工作的 42 列帧会被当成 32 段而拒收。

因此在**无标记**时按列数判定，而不是只看文档默认值：

- `字段数 == 每组列数 × 42` → 按 42 段解析
- `字段数 == 每组列数 × 32` → 按 32 段解析
- 都不匹配 → 整帧丢弃

同一组 `pos`/姿态配置下 32 与 42 的列数从不相等（192/252、224/294、96/126、128/168），
所以该判定无歧义。带显式标记的帧**不做**这种回退，标记说了算，列数不符即丢弃。

### 5.5 解码器改造

`decodeAsGloveCSV_Func` 保持单入口，语义调整为：

- `skeletonCount` 改为「输出数组**容量**」，允许 32 或 42；检测到的段数超过容量则返回失败。
  现有调用方传 42 不受影响。
- 解析结果始终写入调用方给的 `ssd`（容量 42）：32 段帧只填前 32 项，
  按 `kinemHumanHandsSkeleton32Index` 顺序，名称取 `kinemHumanHandsSkeleton32[]`。
- 检测到的格式通过 `sh->skeletonFormat` 回报，调用方据此决定拷进哪个容器。
- `GloveSplitFrameMgr` 增加当前格式字段；**同一帧的各子包标记不一致时整帧丢弃**
  （分包都带同一个帧头，不一致说明串帧或发送端异常）。
- 帧头 `leftFigureSkeletonCount` / `rightFigureSkeletonCount` 按格式填：
  32 段 → 15/15，42 段 → 20/20（`bodySkeletonCount` 均为 2）。这与
  `motionGloveSDK_commonDef.h` 中「默认为 15」的历史注释一致，不再自相矛盾。

### 5.6 返回值

| 返回码 | 含义 |
|---|---|
| `0` | 本帧解析成功，`ssd` 已填充，`sh->skeletonFormat` 有效 |
| `-1` | 参数非法 / 帧头缺 `subpackage` / 子包未收齐 / 列数不符 / 未知 `skeleton_*` |
| `-2` | **新增**：`skeleton_openxr`，格式已识别但布局未定义，整帧丢弃 |

接收线程对 `-2` 做低频告警（首次 + 每若干次一条），避免刷屏。

### 5.7 输出类型与接口

`motionGloveSDKDef.h` 新增 `KHHS32PosAttitude_TypeDef`（与 42 版同构，
`skeletons` 长度为 `KHHS32_SkeletonCount`）；`SingleSkeletonDef` 的匿名 union 内
增加 `kinemHumanHandsSkeleton32Index gloveSkeletonIndex32;`（同为 4 字节，尺寸不变）。

接口层由「一个取帧函数」调整为「一个格式查询 + 三个按格式取帧」，调用方先查格式、
再调对应接口，不再靠返回值猜自己拿到的是哪种布局：

```c
/* 查询某条数据流当前缓存帧的骨骼格式；0 成功，-1 表示流不存在或参数非法。 */
MGSDK_DLL_API int MotionGloveSDK_GetSkeletonsFrameType(
    const char* actorName, GloveStreamSkeletonFormat_TypeDef* pFormat);

/* 42 段：由原名 MotionGloveSDK_GetGloveSkeletonsFrame 改名而来。 */
MGSDK_DLL_API int MotionGloveSDK_GetSkeletons42Frame(
    const char* actorName, KHHS42PosAttitude_TypeDef* pKHHS42PosAttitude);

/* 32 段：新增。 */
MGSDK_DLL_API int MotionGloveSDK_GetSkeletons32Frame(
    const char* actorName, KHHS32PosAttitude_TypeDef* pKHHS32PosAttitude);

/* OpenXR：新增，布局未定义，见 5.9。 */
MGSDK_DLL_API int MotionGloveSDK_GetSkeletonsOpenXRFrame(
    const char* actorName, KHHSOpenXRPosAttitude_TypeDef* pKHHSOpenXRPosAttitude);
```

取帧语义：流存在但格式与接口不符时返回 `-1`（例如对 32 段流的名字调 42 版接口），
不再返回错位数据。**这是对现有 42 取帧行为唯一的变化**，需在头文件注释中写明。

改名影响面（工程内全部同步更新）：

| 位置 | 处理 |
|---|---|
| `MotionGloveSDK/include/motionGloveSDK.h:99` | 声明改名 |
| `MotionGloveSDK/src/motionGloveSDK.cpp:701` | 实现改名 |
| `MotionGloveSDK_examples/src/motionGloveSDK_example2/3/4.cpp` | 调用改名 |
| `MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp:177` | 调用改名，并改用 32 版接口（该示例按 32 段渲染） |
| `MotionGloveSDK_examples/src/motionGloveSDK_example1.cpp:8` | 注释里的函数名 |
| `MotionGloveSDK/inc/eulerToQuat.h:8` | 注释里的函数名 |

`openspec/changes/add-vtk-3d-visualization/` 下的历史文档不再改动。

### 5.9 OpenXR 容器与接口的预留方式

OpenXR 布局未定义（§2.9 规划每手 26、双手 52），本 SDK 当前也收不到该格式。
为了让接口签名一次定妥、将来只补实现，本期先冻结以下形态：

- `motionGloveSDK_skeletonOpenXRDef.h`：`kinemHumanHandsSkeletonOpenXRIndex` 枚举 +
  `KHHSOpenXR_SkeletonCount`。数量取规划值 **52**，注释标明「规划值，布局确定前可能调整」。
- `motionGloveSDKDef.h`：`KHHSOpenXRPosAttitude_TypeDef`，与 42 版同构，
  `skeletons` 长度为 `KHHSOpenXR_SkeletonCount`。
- `MotionGloveSDK_GetSkeletonsOpenXRFrame()`：**恒返回 `-1`**，
  注释写明「布局未定义，当前不会收到该格式；返回 -1 表示不支持」。

该容器只用于占位与类型检查，不参与解码路径；`skeleton_openxr` 帧仍然整帧丢弃并回 `-2`。

### 5.8 管理器存储

`GloveSuitManager_TypeDef` 保留两个并列容器 `posAttitude32` / `posAttitude42`，
并新增 `frameFormat` 标记指明哪一个有效。按最大成员计，与现状内存占用相同（约 8.4 KB × 32 槽），
代价是每槽另多约 6.4 KB（32 槽合计约 205 KB）。

> 实现更正：原设计写的是「按格式区分的 union」，实际不可行——`SingleSkeletonDef` 有用户提供的
> 构造函数，C++11 下含此类成员的 union 其默认构造函数会被删除，`gloveSuitManager` 全局数组
> 将无法构造。故改用两个并列成员。

## 6. 留给我自行决定的部分

- `-2` 的告警节流具体次数。
- union 的成员命名与辅助标记字段的取舍。
- 单测用例拆分粒度。

## 7. 待确认 / 范围外

1. **`isFrameCSV_Glove()` 拒收空演员名**（[motionGloveSdkHelper.cpp:104](../../../MotionGloveSDK/src/motionGloveSdkHelper.cpp)）：
   文档 §2.1/§5.1 写明 `actorName` 默认空串，此时帧以空格开头会被预筛丢弃。
   与本次格式标记无关，属既有缺陷，**本期不改**，需要时单独提。
2. 文档自身两处措辞待订正：§2.9「兼容性要求」仍写「按首 token 识别 `skeleton_42`」；
   §8 说「CSV 行首同样会带 `skeleton_42` 标记」（实际在帧头内部）。
   本次只修改接收端实现与必要注释，正文由文档所有者订正。
3. 旧名 `MotionGloveSDK_GetGloveSkeletonsFrame` **不保留宏别名**，按改名处理，
   工程内 7 处调用/注释全部更新（清单见 5.7）。
4. OpenXR 容器数量冻结为规划值 **52**（见 5.9）。若最终布局不是 52，需同步改
   `KHHSOpenXR_SkeletonCount` 与容器定义——属于预期的后续调整，不是返工。
