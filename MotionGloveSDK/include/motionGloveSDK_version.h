/**
 * @file  motionGloveSDK_version.h
 * @brief 示例发布包（MotionGloveSDK_example）的版本号定义。
 * @ingroup mgsdk_examples
 *
 * @details
 * 这里是示例发布版本的唯一来源：CMake 用它设置工程版本，示例启动时会打印，
 * 发布工作流（.github/workflows/release.yml）会校验它与 tag 一致。
 *
 * 发版流程：修改下面的版本号 -> 提交 -> `git tag vX.Y.Z` -> `git push origin vX.Y.Z`。
 * 标签与版本号不一致时发布流水线会直接失败，避免产物名与二进制内的版本对不上。
 */
#pragma once

/** @brief 主版本号。 */
#define MOTIONGLOVE_EXAMPLE_VERSION_MAJOR 0
/** @brief 次版本号。 */
#define MOTIONGLOVE_EXAMPLE_VERSION_MINOR 2
/** @brief 修订号。 */
#define MOTIONGLOVE_EXAMPLE_VERSION_PATCH 1

#define MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE_(x) #x
#define MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE(x) MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE_(x)

/** @brief 版本号字符串，形如 "0.2.1"。 */
#define MOTIONGLOVE_EXAMPLE_VERSION_STRING      \
    MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE(MOTIONGLOVE_EXAMPLE_VERSION_MAJOR) "." \
    MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE(MOTIONGLOVE_EXAMPLE_VERSION_MINOR) "." \
    MOTIONGLOVE_EXAMPLE_VERSION_STRINGIZE(MOTIONGLOVE_EXAMPLE_VERSION_PATCH)

/** @brief 版本号的数值形式（MAJOR*10000 + MINOR*100 + PATCH），便于比较大小。 */
#define MOTIONGLOVE_EXAMPLE_VERSION_NUMBER      \
    (MOTIONGLOVE_EXAMPLE_VERSION_MAJOR * 10000 + \
     MOTIONGLOVE_EXAMPLE_VERSION_MINOR * 100 +   \
     MOTIONGLOVE_EXAMPLE_VERSION_PATCH)
