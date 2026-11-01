/**
 * @file  motionGloveSDK_skeleton32Def.h
 * @brief 32 段手部骨骼定义：骨骼编号枚举与骨骼名称表。
 * @ingroup mgsdk_types
 *
 * @details
 * 模块边界：纯数据类型（枚举 + const std::string 名称表），无实现。
 * 32 段协议为旧版手部骨骼；当前 SDK 主流为 42 段
 * （见 motionGloveSDK_skeleton42Def.h），两者叠用见 decoded 数据结构。
 */
#pragma once
#include <stdint.h>
#include <string>


/** @brief 32 段手部骨骼编号：索引 0~15 为右手（手掌 + 5 指 x 3），16~31 为左手；KHHS32_SkeletonCount = 32。 */
typedef enum : uint32_t
{

	KHHS32_RightHand,    //对应右手传感器原始数据

	KHHS32_RightHandThumb1,
	KHHS32_RightHandThumb2,
	KHHS32_RightHandThumb3,
	KHHS32_RightHandIndex1,
	KHHS32_RightHandIndex2,
	KHHS32_RightHandIndex3,
	KHHS32_RightHandMiddle1,
	KHHS32_RightHandMiddle2,
	KHHS32_RightHandMiddle3,
	KHHS32_RightHandRing1,
	KHHS32_RightHandRing2,
	KHHS32_RightHandRing3,
	KHHS32_RightHandPinky1,
	KHHS32_RightHandPinky2,
	KHHS32_RightHandPinky3,

	KHHS32_LeftHand,	   //对应左手传感器原始数据

	KHHS32_LeftHandThumb1,
	KHHS32_LeftHandThumb2,
	KHHS32_LeftHandThumb3,
	KHHS32_LeftHandIndex1,
	KHHS32_LeftHandIndex2,
	KHHS32_LeftHandIndex3,
	KHHS32_LeftHandMiddle1,
	KHHS32_LeftHandMiddle2,
	KHHS32_LeftHandMiddle3,
	KHHS32_LeftHandRing1,
	KHHS32_LeftHandRing2,
	KHHS32_LeftHandRing3,
	KHHS32_LeftHandPinky1,
	KHHS32_LeftHandPinky2,
	KHHS32_LeftHandPinky3,

	KHHS32_SkeletonCount
}kinemHumanHandsSkeleton32Index;

/** @brief 32 段骨骼全称名称表：索引与 #kinemHumanHandsSkeleton32Index 枚举一一对应。 */
const std::string kinemHumanHandsSkeleton32[KHHS32_SkeletonCount] = {

	"RightHand",

	"RightHandThumb1",
	"RightHandThumb2",
	"RightHandThumb3",
	"RightHandIndex1",
	"RightHandIndex2",
	"RightHandIndex3",
	"RightHandMiddle1",
	"RightHandMiddle2",
	"RightHandMiddle3",
	"RightHandRing1",
	"RightHandRing2",
	"RightHandRing3",
	"RightHandPinky1",
	"RightHandPinky2",
	"RightHandPinky3",


	"LeftHand",

	"LeftHandThumb1",
	"LeftHandThumb2",
	"LeftHandThumb3",
	"LeftHandIndex1",
	"LeftHandIndex2",
	"LeftHandIndex3",
	"LeftHandMiddle1",
	"LeftHandMiddle2",
	"LeftHandMiddle3",
	"LeftHandRing1",
	"LeftHandRing2",
	"LeftHandRing3",
	"LeftHandPinky1",
	"LeftHandPinky2",
	"LeftHandPinky3"
};

/** @brief 32 段骨骼短名（缩写）名称表：索引与 #kinemHumanHandsSkeleton32Index 枚举一一对应。 */
const std::string kinemHumanHandsSkeleton32Short[KHHS32_SkeletonCount] = {

	"rHAND",

	"RHT1",
	"RHT2",
	"RHT3",
	"RHI1",
	"RHI2",
	"RHI3",
	"RHM1",
	"RHM2",
	"RHM3",
	"RHR1",
	"RHR2",
	"RHR3",
	"RHP1",
	"RHP2",
	"RHP3",


	"lHAND",

	"LHT1",
	"LHT2",
	"LHT3",
	"LHI1",
	"LHI2",
	"LHI3",
	"LHM1",
	"LHM2",
	"LHM3",
	"LHR1",
	"LHR2",
	"LHR3",
	"LHP1",
	"LHP2",
	"LHP3"
};

