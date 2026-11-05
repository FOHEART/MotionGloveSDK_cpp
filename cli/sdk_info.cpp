/**
 * @file  sdk_info.cpp
 * @brief 版本号读取实现。
 * @ingroup mgsdk_cli
 */
#include "sdk_info.h"

#include <cstdio>

#include "motionGloveSDK.h"
#include "motionGloveSDK_version.h"

namespace mgcli {

unsigned int sdkVersionRaw()
{
	return MotionGloveSDK_getVersion();
}

std::string sdkVersionString()
{
	const unsigned int version = sdkVersionRaw();
	char buffer[32] = { 0 };
	snprintf(buffer, sizeof(buffer), "%u.%u.%u",
	         (version >> 16) & 0xFFu, (version >> 8) & 0xFFu, version & 0xFFu);
	return buffer;
}

std::string cliVersionString()
{
	return MOTIONGLOVE_SDK_VER_STRING;
}

} // namespace mgcli
