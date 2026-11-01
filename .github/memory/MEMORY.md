# MotionGloveSDK — Claude Memory

## 项目概况
- CMake 工程，顶层构建 SDK 共享库（`MotionGloveSDK`）+ 示例可执行文件
- 支持平台：Windows（VS2022/VS2026）、Linux/Ubuntu
- C++11，共享库编译（SHARED）

## 关键路径
- 顶层 CMake：`CMakeLists.txt`
- SDK 子库 CMake：`MotionGloveSDK/CMakeLists.txt`
- 公开头文件：`MotionGloveSDK/include/`
- 内部头文件：`MotionGloveSDK/inc/`
- SDK 源文件：`MotionGloveSDK/src/`
- 示例源文件：`MotionGloveSDK_examples/src/*example*.cpp`（CMake 自动 glob）
- 构建输出：`out/build/x64-Debug/`、`out/build/x64-Release/`
- 安装输出：`out/install/x64-Debug/`、`out/install/x64-Release/`

## 构建脚本
| 文件 | 用途 |
|------|------|
| `[Windows]build_vs2022.bat` | VS2022 一键 Debug+Release 构建 |
| `[Windows]build_vs2026.bat` | VS2026 一键 Debug+Release 构建（生成器 "Visual Studio 18 2026"） |
| `[Linux]build.sh` | Linux 一键 Debug+Release 构建 |

## Linux 脚本注意事项
- hgfs 共享文件夹写入的脚本会带有 Windows CRLF 换行符，需用 `tr -d '\r'` 去除（`sed -i` 在 hgfs 上会失败，需借助临时文件）
- hgfs 环境下 `make` 会报时钟偏差警告，纯警告不影响构建
- Linux 用 `-DCMAKE_BUILD_TYPE=Debug/Release`（单配置生成器），不用 `--config` 参数
- Windows 用 `-DCMAKE_CONFIGURATION_TYPES` + `--config Debug/Release`（多配置生成器）

## CMake 构建产物
- Windows：`MotionGloveSDK.dll` + `MotionGloveSDK.lib`
- Linux：`libMotionGloveSDK.so`

## 离线工具箱（autotools）
- 完整清单与命令模板：`.github/skills/autotools/SKILL.md`（工具本体在 `D:\autotools`，Python 在 `C:\python`，均为绿色版）
- 常用：pandoc / wkhtmltopdf / ffmpeg / wget / aria2c / mutool / pdftotext / 7za / doxygen / gallery-dl
- PATH 注入（仅当前终端会话）：`call tools\autotools_env.bat`
- 已知缺口：`D:\autotools\gost\gost.exe` 缺失，Gost 暂不可用

## 质量流程（构建 / 测试 / 文档）
| 目标 | 命令 |
|------|------|
| 构建 + 单元测试 | `tools\build_and_test.bat [Debug\|Release] [gtest 过滤正则]` |
| 生成 API 文档 | `gen_docs.bat`（产出 `docs\html\index.html`） |

- 测试接入方式、19 个用例的红→绿过程与验收结果详见 `tests/README.md`
- 测试与文档均以「零告警」为验收线：`ctest` 全绿、`doxygen` 退出码 0 且 `doxygen_warnings.log` 为 0 字节
- 如需脚本化（可直接进 CI）：`D:\autotools\skills\script\` 下的 `ctest_run.py`、`gen_docs.py`、`doxygen_doc_audit.py`

## Doxygen 注释约定（WARN_AS_ERROR=YES 下必须遵守）
- `@cond` 后只能跟标签名，**不能带说明文字**（否则被当条件表达式求值报错）
- 纯测试文件整段用 `/// @cond DOC_HIDDEN` … `/// @endcond` 包裹，避免被测头文件在测试翻译单元中重复扫描
- 只用标准 `@brief`，不要自造 `@brief_cn` / `@brief_en`
- 单个文件只写一个 `@file`
- `#宏名` 引用之后**不要紧跟中文句号**
- 结构体成员用 `/* */` 注释不算文档；`}A, *B;` 同一行多个 typedef 只认领第一个
- `Doxyfile` 的 `INPUT` 收窄为 `tests/unit`（若含 `tests/README.md` 的一级标题，会与 `USE_MDFILE_AS_MAINPAGE` 冲突报「found more than one \mainpage」）
- `INPUT` 纳入的 Markdown 用 `###` 及以下标题；`####` 会被解析为 subsubsection 而告警
- `WARN_AS_ERROR=YES` **每轮只报第一条告警**，改完需重跑；按清单批量预补可减少轮次

## 环境备注
- 沙箱环境屏蔽 `vcvars64.bat`（reg.exe 黑名单）；此时需手动注入 MSVC 14.44.35207 + Win SDK 10.0.26100.0 环境变量，并用 NMake 生成器构建到 `out/build/tdd-nmake`
- 常规环境直接跑 `tools\build_and_test.bat`（VS2022 生成器，构建目录 `out\build\tdd-tests`）与 `gen_docs.bat` 即可
- GoogleTest 离线源码：`D:\deps\googletest-1.14.0`，配置时可用 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<路径>`
- 工程内 `.bat` 脚本均为 UTF-8（无 BOM），`build_and_test.bat` / `gen_docs.bat` 内含 `chcp 65001`；用 PowerShell `Get-Content` 直接读会按 ANSI 解码而显示乱码，属读取方式问题，文件本身正常
