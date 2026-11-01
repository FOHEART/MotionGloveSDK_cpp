/**
 * @file  motionGloveSDK_skeletonOpenXRDef.h
 * @brief OpenXR 关节布局的占位定义：关节数量与索引枚举。
 * @ingroup mgsdk_types
 *
 * @details
 * 模块边界：纯数据类型（枚举），无实现、无名称表。
 * 当前状态：OpenXR 布局尚未定义（规划每手 26 个关节、双手 52 个），
 * SDK 也还收不到该格式的数据；本文件仅用于冻结公开容器的尺寸，
 * 使其在布局确定后无需改动 ABI 即可补齐实现。
 *
 * 约定：该格式的字段排布未写入
 * motionGloveSDK/include/MOTIONGLOVE_SDK_STREAM_FORMAT.md 之前，
 * 接收端一律整帧丢弃，不得按 32 段或 42 段猜测解析。
 */
#pragma once
#include <stdint.h>

/**
 * @brief OpenXR 关节编号与数量：索引范围 [0, KHHSOpenXR_SkeletonCount)。
 * @details 具体顺序待布局确定后补充；当前仅有数量，供容器尺寸使用。
 */
typedef enum : uint32_t
{
	KHHSOpenXR_SkeletonIndexBegin = 0,  ///< 关节索引起点（布局未定义，仅占位）
	KHHSOpenXR_SkeletonCount = 52       ///< 关节数量：规划每手 26 个、双手 52；布局确定前可能调整
} kinemHumanHandsSkeletonOpenXRIndex;
