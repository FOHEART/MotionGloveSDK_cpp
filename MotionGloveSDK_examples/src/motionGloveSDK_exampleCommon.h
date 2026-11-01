/**
 * @file  motionGloveSDK_exampleCommon.h
 * @brief 示例程序共用的终端打印工具：统一行首（帧序号 + 骨骼段数）。
 * @ingroup mgsdk_examples
 *
 * @details
 * 各示例在打印逐帧数据前先输出统一的行首，形如 "[1234][42] "：
 *  - 第一个中括号是当前帧的帧序号，取自帧头 hdr.frameNumber；
 *  - 第二个中括号是本帧的骨骼段数，取值为 32 / 42 / OpenXR。
 * 行首自带一个尾随空格，示例在其后直接续写自己的消息文本。
 */
#pragma once

#include <stdio.h>
#include "motionGloveSDK.h"

/**
 * @brief 骨骼段数标签：把骨骼格式枚举转换成打印用的段数文本。
 * @param format [in] 本帧的骨骼格式。
 * @return "32" / "42" / "OpenXR"；非法枚举值返回 "?"。
 */
inline const char* GloveSkeletonSegmentLabel(GloveStreamSkeletonFormat_TypeDef format)
{
    switch (format)
    {
    case GloveStreamFormat_Skeleton32: return "32";
    case GloveStreamFormat_Skeleton42: return "42";
    case GloveStreamFormat_OpenXR:     return "OpenXR";
    default:                           return "?";
    }
}

/**
 * @brief 打印统一行首 "[帧序号][骨骼段数] "。
 * @param hdr [in] 当前帧的帧头，由 MotionGloveSDK_GetSkeletons32Frame /
 *             MotionGloveSDK_GetSkeletons42Frame 取回的帧数据中的 hdr 成员。
 */
inline void PrintGloveFrameHeader(const StreamHeader& hdr)
{
    printf("[%u][%s] ",
        static_cast<unsigned int>(hdr.frameNumber),
        GloveSkeletonSegmentLabel(hdr.skeletonFormat));
}
