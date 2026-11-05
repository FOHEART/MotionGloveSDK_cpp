/**
 * @file  sdk_info.h
 * @brief 版本号读取：SDK 版本（DLL 内）与 CLI/示例发布版本（版本头文件）。
 * @ingroup mgsdk_cli
 */
#pragma once

#include <string>

namespace mgcli {

/** @brief MotionGloveSDK_getVersion() 的原始 32 位值。 */
unsigned int sdkVersionRaw();

/** @brief SDK 版本字符串，形如 "0.0.9"。 */
std::string sdkVersionString();

/** @brief CLI 版本字符串，取自 motionGloveSDK_version.h，形如 "0.2.1"。 */
std::string cliVersionString();

} // namespace mgcli
