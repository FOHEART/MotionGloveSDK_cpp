/**
 * @file  motionGloveSDK_version.h
 * @brief 发布包（MotionGloveSDK + 示例 + CLI）的版本号定义。
 * @ingroup mgsdk_api
 *
 * @details
 * 这里是发布版本的唯一来源：CMake 用它设置工程版本，示例/CLI 启动时会打印，
 * 发布工作流（.github/workflows/release.yml）会校验它与 tag 一致。
 *
 * 它与 API 里的 MotionGloveSDK_getVersion()（ABI 运行时版本，当前 0.0.9）
 * 是两套号，互不联动；发版流程只改本文件里的三个版本宏。
 *
 * 发版流程：修改下面的版本号 -> 提交 -> `git tag vX.Y.Z` -> `git push origin vX.Y.Z`。
 * 标签与版本号不一致时发布流水线会直接失败，避免产物名与二进制内的版本对不上。
 */
/* 用经典 include guard 而不是 #pragma once：资源编译器（rc.exe）会预处理本文件 */
#ifndef MOTIONGLOVE_SDK_VERSION_H
#define MOTIONGLOVE_SDK_VERSION_H

/** @brief 主版本号。 */
#define MOTIONGLOVE_SDK_VER_MAJOR 0
/** @brief 次版本号。 */
#define MOTIONGLOVE_SDK_VER_MINOR 2
/** @brief 修订号。 */
#define MOTIONGLOVE_SDK_VER_PATCH 1

/** @brief 内部辅助宏：把参数字符串化（不带扩展的两段式写法）。 */
#define MOTIONGLOVE_SDK_VER_STRINGIZE_(x) #x
/** @brief 把 MOTIONGLOVE_SDK_VER_MAJOR 等宏展开后的数值转成字符串字面量。 */
#define MOTIONGLOVE_SDK_VER_STRINGIZE(x) MOTIONGLOVE_SDK_VER_STRINGIZE_(x)

/** @brief 版本号字符串，形如 "0.2.1"。 */
#define MOTIONGLOVE_SDK_VER_STRING      \
    MOTIONGLOVE_SDK_VER_STRINGIZE(MOTIONGLOVE_SDK_VER_MAJOR) "." \
    MOTIONGLOVE_SDK_VER_STRINGIZE(MOTIONGLOVE_SDK_VER_MINOR) "." \
    MOTIONGLOVE_SDK_VER_STRINGIZE(MOTIONGLOVE_SDK_VER_PATCH)

/** @brief 版本号的数值形式（MAJOR*10000 + MINOR*100 + PATCH），便于比较大小。 */
#define MOTIONGLOVE_SDK_VER_NUMBER      \
    (MOTIONGLOVE_SDK_VER_MAJOR * 10000 + \
     MOTIONGLOVE_SDK_VER_MINOR * 100 +   \
     MOTIONGLOVE_SDK_VER_PATCH)

#endif /* MOTIONGLOVE_SDK_VERSION_H */
