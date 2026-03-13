#pragma once
#include <stdint.h>
#include <string>


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

