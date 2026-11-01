/**
 * @file  motionGloveSDK_skeleton42Def.h
 * @brief 42 段手部骨骼定义：编号枚举（KHHS42_*）与骨骼名称表。
 * @ingroup mgsdk_types
 *
 * @details
 * 模块边界：纯数据类型（枚举 + const std::string 名称表），无实现。
 * 42 段 = 右手 21 段（手掌 + 5 指 x 4）+ 左手 21 段；
 * 编号顺序即骨骼索引 KHHS42_SkeletonCount = 42。
 */
#pragma once
#include <stdint.h>
#include <string>

/** @brief 42 段手部骨骼编号：索引 0~20 为右手（手掌 + 5 指 x 4，含指尖末端），21~41 为左手；KHHS42_SkeletonCount = 42。 */
typedef enum : uint32_t
{

	KHHS42_RightHand, // 对应右手传感器原始数据

	KHHS42_RightHandThumb1,
	KHHS42_RightHandThumb2,
	KHHS42_RightHandThumb3,
	KHHS42_RightHandThumb3End,
	KHHS42_RightHandIndex1,
	KHHS42_RightHandIndex2,
	KHHS42_RightHandIndex3,
	KHHS42_RightHandIndex3End,
	KHHS42_RightHandMiddle1,
	KHHS42_RightHandMiddle2,
	KHHS42_RightHandMiddle3,
	KHHS42_RightHandMiddle3End,
	KHHS42_RightHandRing1,
	KHHS42_RightHandRing2,
	KHHS42_RightHandRing3,
	KHHS42_RightHandRing3End,
	KHHS42_RightHandPinky1,
	KHHS42_RightHandPinky2,
	KHHS42_RightHandPinky3,
	KHHS42_RightHandPinky3End,

	KHHS42_LeftHand, // 对应左手传感器原始数据

	KHHS42_LeftHandThumb1,
	KHHS42_LeftHandThumb2,
	KHHS42_LeftHandThumb3,
	KHHS42_LeftHandThumb3End,
	KHHS42_LeftHandIndex1,
	KHHS42_LeftHandIndex2,
	KHHS42_LeftHandIndex3,
	KHHS42_LeftHandIndex3End,
	KHHS42_LeftHandMiddle1,
	KHHS42_LeftHandMiddle2,
	KHHS42_LeftHandMiddle3,
	KHHS42_LeftHandMiddle3End,
	KHHS42_LeftHandRing1,
	KHHS42_LeftHandRing2,
	KHHS42_LeftHandRing3,
	KHHS42_LeftHandRing3End,
	KHHS42_LeftHandPinky1,
	KHHS42_LeftHandPinky2,
	KHHS42_LeftHandPinky3,
	KHHS42_LeftHandPinky3End,

	KHHS42_SkeletonCount
} kinemHumanHandsSkeleton42Index;

/** @brief 42 段骨骼全称名称表：索引与 #kinemHumanHandsSkeleton42Index 枚举一一对应。 */
const std::string kinemHumanHandsSkeleton42[KHHS42_SkeletonCount] = {

	"RightHand",

	"RightHandThumb1",
	"RightHandThumb2",
	"RightHandThumb3",
	"RightHandThumb3End",
	"RightHandIndex1",
	"RightHandIndex2",
	"RightHandIndex3",
	"RightHandIndex3End",
	"RightHandMiddle1",
	"RightHandMiddle2",
	"RightHandMiddle3",
	"RightHandMiddle3End",
	"RightHandRing1",
	"RightHandRing2",
	"RightHandRing3",
	"RightHandRing3End",
	"RightHandPinky1",
	"RightHandPinky2",
	"RightHandPinky3",
	"RightHandPinky3End",

	"LeftHand",

	"LeftHandThumb1",
	"LeftHandThumb2",
	"LeftHandThumb3",
	"LeftHandThumb3End",
	"LeftHandIndex1",
	"LeftHandIndex2",
	"LeftHandIndex3",
	"LeftHandIndex3End",
	"LeftHandMiddle1",
	"LeftHandMiddle2",
	"LeftHandMiddle3",
	"LeftHandMiddle3End",
	"LeftHandRing1",
	"LeftHandRing2",
	"LeftHandRing3",
	"LeftHandRing3End",
	"LeftHandPinky1",
	"LeftHandPinky2",
	"LeftHandPinky3",
	"LeftHandPinky3End"};
