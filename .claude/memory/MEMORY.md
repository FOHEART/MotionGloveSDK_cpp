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
