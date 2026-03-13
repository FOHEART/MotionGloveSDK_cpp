#pragma once
#include "stdint.h"
#include "include/motionGloveSDKDef.h"

int decodeAsGloveCSV_Func(char* binStream,
	StreamHeader* sh,
	SingleSkeletonDef* ssd,
	unsigned int skeletonCount);