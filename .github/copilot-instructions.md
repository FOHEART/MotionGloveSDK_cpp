# MotionGloveSDK (C++) — Copilot 指南

动捕手套 SDK：UDP 接收 MotionGlove 上位机转发的**文本（CSV）**数据流，解帧成 32 / 42 / OpenXR（预留）三种骨骼布局，
以 `extern "C"` 接口提供"查询新帧 → 取帧"的拉取式 API，并附带示例程序、VTK 3D 示例、GoogleTest 单元测试与 Doxygen 文档。

仓库内的源码注释、文档、提交信息均使用中文，新增内容保持中文。

## 构建 / 测试 / 文档

| 目的 | 命令 |
| --- | --- |
| 构建 + 跑全部单元测试（推荐） | `tools\build_and_test.bat`（默认 Debug；`tools\build_and_test.bat Release` 指定配置） |
| 只跑匹配的用例 | `tools\build_and_test.bat Debug "Unit.EulerToQuat.*"`（第二个参数即 `ctest -R` 正则） |
| 一键全量构建（Debug+Release+安装+汇总示例） | `[Windows]build_vs2022.bat` / `[Windows]build_vs2026.bat` / `bash "[Linux]build.sh"` |
| 手动构建 | `cmake -S . -B out/build/x64-Debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug"`，再 `cmake --build out/build/x64-Debug --config Debug` |
| 文档零告警验收 | `gen_docs.bat`（Doxygen `WARN_AS_ERROR=YES`；退出码 0 且 `doxygen_warnings.log` 为 0 字节才算过） |

- 测试经 `gtest_discover_tests(TEST_PREFIX "Unit.")` 注册为独立 CTest，名称形如 `Unit.<TestSuite>.<TestName>`；
  也可直接 `ctest --test-dir out/build/tdd-tests -C Debug -R "Unit.decodeAsGloveCSV.*" --output-on-failure`。
- 本工程没有 clang-format/clang-tidy 等 linter，**Doxygen 零告警就是事实上的 lint 门禁**。
- `tools\build_and_test.bat` 自行 `call` VS2022 的 `vcvars64.bat`（硬编码 `C:\Program Files\Microsoft Visual Studio\2022\Community\...`），
  构建目录是 `out\build\tdd-tests`；`CMakeSettings.json` 用 Ninja 生成器写 `out\build\x64-Debug`，两者互不干扰，不要混用。
- 网络受限时 GoogleTest 可用离线源码：配置时加 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=D:\deps\googletest-1.14.0`。
- VTK 示例默认编译（`BUILD_VTK_EXAMPLE=ON`），找不到 VTK 9.x 时 CMake 只告警并跳过该目标；`tools\setup_vtk.bat` 可自动装 conda-forge VTK 到 `third_party\vtk`，
  且默认按最小依赖安装（`vtk-base` + `vtk-io-ffmpeg` 用 `--no-deps`，约 490 MB / 7200 文件；`VTK_FULL=1` 才是完整 2.2 GB 安装），切换模式需 `VTK_FORCE_REINSTALL=1`。

## 架构

数据通路（`MotionGloveSDK/src/`）：

```
UDP 收包线程 (motionGloveSDK.cpp: ThreadProc)
  └ select(10ms) → 非阻塞 recvfrom 收空队列        # 不要改回"每包 Sleep"，会丢 60Hz 大帧
      └ isFrameCSVFormat / isFrameCSV_Glove        # 帧以 "Glove" 开头才当手套流
          └ decodeAsGloveCSV_Func                  # 解帧：帧头分词 + subpackage 重组 + 布局判定
              └ gloveSuitManager[32] 槽位 (ReadWriteLock)
                  └ GloveSuitManager_TypeDef: posAttitude32 / posAttitude42 + frameFormat
                      └ 公开取帧接口 (motionGloveSDK.h)
```

- **拉取式 API 配对**：`MotionGloveSDK_ListenUDPPort` → 循环 `isGloveNewFramePending(name)` → `GetSkeletonsFrameType(name, &fmt)`
  → 按格式调 `GetSkeletons32Frame` / `GetSkeletons42Frame` → `resetGloveNewFramePending(name)`；`actorName` 是数据流名（示例用 `"Glove1"`，上限 63 字节）。
- **32 段与 42 段各自原生**，不做上采样：两者是并列容器 + 独立结构体/接口；格式不符时取帧返回 -1，不会给出错位数据。
- **布局判定**（`decodeAsGloveCSV.cpp`）：帧头 `fn <帧号>` 后的 `skeleton_32` / `skeleton_42` / `skeleton_openxr` 标记优先；
  无标记时按列数回退（`42×每段列数` 或 `32×每段列数`），都不匹配则整帧丢弃。带标记但列数不符同样丢弃。
- **分包重组**：内部是进程级静态 `GloveSplitFrameMgr`，按帧号累积 `subpackage i/N`；返回 -1 表示"子包还没收齐"，
  是正常流程而非错误。同一帧各子包标记不一致要整帧丢弃。
- **OpenXR 只冻结了 ABI**：`KHHSOpenXRPosAttitude_TypeDef` 与 `GetSkeletonsOpenXRFrame()` 恒返回 -1；
  解码器遇到 `skeleton_openxr` 返回 `MGSDK_DECODE_OPENXR_UNSUPPORTED`（-2），接收线程低频告警后丢帧。
- **示例自动生成目标**：`MotionGloveSDK_examples/src/*example*.cpp` 由 `file(GLOB)` 每个文件一个可执行目标（名字取文件主干）；
  新增示例只要按此命名落盘即可。`*_vtk.cpp` 受 `BUILD_VTK_EXAMPLE` 控制。POST_BUILD 会把 `MotionGloveSDK.dll` 与 VTK 运行库
  复制到 exe 同目录（`cmake/copy_runtime_dlls.cmake`）。
- **版本号唯一来源**：`MotionGloveSDK_examples/inc/motionGloveSDK_version.h`。顶层 `CMakeLists.txt` 解析它设置工程版本，
  示例启动时打印，`.github/workflows/release.yml` 校验 `vX.Y.Z` 标签与它一致——改版本只改这一个文件。

协议与设计依据（改动前先读）：`MotionGloveSDK/include/MOTIONGLOVE_SDK_STREAM_FORMAT_2026-11-02.md`（数据流格式权威文档）、
`docs/superpowers/specs/2026-11-01-multiformat-stream-decode-design.md`（32/42/OpenXR 多格式解码决策）。

## 关键约定

- **Doxygen 注释是硬约束**（`WARN_AS_ERROR=YES`）：只用标准标签（不要自造 `@brief_cn`）；一个文件只写一个 `@file`；
  `@cond` 后只能跟标签名（如 `DOC_HIDDEN`），不能带说明文字；`#宏名` 后不要紧跟中文标点；`WARN_AS_ERROR` 每轮只报第一条告警，改完需重跑。
- **ABI 尺寸稳定**：`motionGloveSDK_commonDef.h` / `motionGloveSDKDef.h` 全部 `#pragma pack(1)`，枚举固定 `uint8_t`；
  给 `StreamHeader` 加字段要用 `resv` 里的空间（新增 `skeletonFormat` 就是这样做到 `sizeof` 不变的）。
- **C++11 的 SDK 本体**：`MotionGloveSDK/` 与示例保持 C++11；只有 `tests/unit/` 提升到 C++14（GoogleTest v1.14 的最低要求）。
  含用户构造函数的结构体（如 `SingleSkeletonDef`）不能放进 union 的默认构造路径，所以管理器用的是并列成员而非 union。
- **测试只能编译纯逻辑源文件**：`tests/unit/CMakeLists.txt` 只把 `eulerToQuat.cpp`、`decodeAsGloveCSV.cpp` 编进 `mgsdk_testlib`，
  避免链接未导出的 DLL 符号；新增被测模块同理，只加纯逻辑文件。
- **测试写法**：浮点比较只能用 `EXPECT_NEAR_FLOAT(actual, expected, eps)`（`tests/unit/gtest_assertions.h`）；
  解码器有静态帧状态，用例之间必须使用**互不相同的帧号**；线程用例用 `std::async` + `future.wait_for` 限时，防止挂死。
- **接收线程性能敏感**：`SO_RCVBUF` 为 1MB，`select` + 非阻塞收包是修过"骨骼不动/跳帧"的关键，不要加 `Sleep`/阻塞式 `recvfrom`。
- Windows 编码：编译选项含 `/utf-8`、`UNICODE`；`.bat` 脚本是 UTF-8 无 BOM 且自带 `chcp 65001`，
  用 PowerShell `Get-Content` 读会按 ANSI 误解码（显示乱码），是读取方式问题。
- 变更流程走 OpenSpec：`openspec/` 下是活动变更与规格，见 `.github/agents/openspec.agent.md` 与 `.github/skills/openspec-*`。

## 离线工具箱（autotools，按需调用）

本机 `D:\autotools` 与 `C:\python` 是绿色版工具箱（清单来源 `D:\autotools\autotools.md`，详细命令模板见
`.github/skills/autotools/SKILL.md`）。以下工具可直接按需使用，无需安装：

| 工具 | 用途 | 主程序路径 |
| --- | --- | --- |
| pandoc | Markdown ↔ Word/HTML/PDF 等格式转换 | `D:\autotools\Pandoc\pandoc.exe` |
| wkhtmltopdf | HTML → PDF / 图片（`wkhtmltoimage.exe`） | `D:\autotools\wkhtmltopdf\bin\wkhtmltopdf.exe` |
| ffmpeg | 音视频转码、剪辑、`ffprobe` 探针 | `D:\autotools\ffmpeg\bin\ffmpeg.exe` |
| python | Python 3 运行环境 | `C:\python\python.exe` |
| Gost | 端口转发 / 代理隧道 | `D:\autotools\gost\`（`gost.exe` 本体缺失，暂不可用） |
| wget | 命令行单文件下载 | `D:\autotools\wget\wget.exe` |
| aria2c | 多线程/多协议下载（HTTP/FTP/BT/磁力） | `D:\autotools\aria2-1.37.0-win-64bit-build1\aria2c.exe` |
| mutool | PDF 查看 / 合并 / 拆分 / 清洗 | `D:\autotools\mupdf\mutool.exe` |
| iperf3 | 网络带宽与性能测试 | `D:\autotools\iperf\iperf3.exe` |
| Poppler | `pdftotext` / `pdftoppm` / `pdftocairo` / `pdfimages` / `pdfinfo` | `D:\autotools\pdftohtml\` |
| 7za | 命令行压缩 / 解压（7z/zip/gz/xz/tar；ARM64 见 `arm64\`） | `D:\autotools\7z\7za.exe` |
| doxygen | 源码文档生成（`doxywizard` / `doxyindexer` 同目录；`doxysearch.exe` 缺失） | `D:\autotools\doxygen-1.11.0\doxygen.exe` |
| gallery-dl | 图集下载（1000+ 站点） | `D:\autotools\gallery_dl\gallery-dl.exe` |
| Crawl4AI | 网页爬取与结构化提取（技能包） | `D:\autotools\crawl4ai\SKILL.md` |
| RhinoAI | Rhino 8 三维建模 MCP 配置包 | `D:\autotools\rhinoai\SKILL.md` |
| claude-skills | 参考技能包（pdf / pptx / video-downloader） | `D:\autotools\claude-skills\` |

- 想用短命令时先在 cmd 里 `call tools\autotools_env.bat`（只改当前会话 PATH，含 python）。
- 本机未安装加密软件：新建/落地文件直接写盘，无需任何中转或加密处理。
