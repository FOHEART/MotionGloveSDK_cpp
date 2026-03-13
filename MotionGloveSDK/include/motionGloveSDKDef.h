#pragma once
#include <stdint.h>
#include <cstring>
#include "motionGloveSDK_commonDef.h"
#include "motionGloveSDK_HMAXGloveDef.h"


#define ACTOR_NAME_LEN_MAX (64)

typedef enum {
	SDK_Status_None,
	SDK_Status_OpenUDP,
	SDK_Status_ThreadRunning,
	SDK_Status_ThreadPause

}SDK_Status_TypeDef;

#pragma pack(1)

/*
人体单段骨骼的数据定义 ，与本段骨骼所有相关的数据都存储在里面
*/
typedef struct _SingleSkeleton {
	_SingleSkeleton()
	{
		gloveSkeletonIndex = (kinemHumanHandsSkeleton32Index)(-1);

		/*53段或者32段骨骼的相关数据*/
		memset(skeletonName, 0x00, sizeof(skeletonName));
		memset(position_meter, 0x00, sizeof(position_meter));
		memset(quat_xyzw, 0x00, sizeof(quat_xyzw));
		quat_xyzw[0] = 0.0f;
		quat_xyzw[1] = 0.0f;
		quat_xyzw[2] = 0.0f;
		quat_xyzw[3] = 1.0f;
		memset(euler_degree, 0x00, sizeof(euler_degree));

		/*相关的传感器原始数据*/
		memset(sensorQuat_xyzw, 0x00, sizeof(sensorQuat_xyzw));

		sensorQuat_xyzw[0] = 0.0f;
		sensorQuat_xyzw[1] = 0.0f;
		sensorQuat_xyzw[2] = 0.0f;
		sensorQuat_xyzw[3] = 1.0f;
		memset(sensorEuler_degree, 0x00, sizeof(sensorEuler_degree));
		memset(sensorAccel_g, 0x00, sizeof(sensorAccel_g));
		memset(sensorLAccel_g, 0x00, sizeof(sensorLAccel_g));
		memset(sensorGyro_DegreePerSecond, 0x00, sizeof(sensorGyro_DegreePerSecond));
		memset(sensorMag_mGauss, 0x00, sizeof(sensorMag_mGauss));
	}
	union
	{
		kinemHumanHandsSkeleton32Index gloveSkeletonIndex;
	};


	char skeletonName[64];

	/*
	下面三个为一组 输出为人体53段骨骼的位置与旋转信息
	坐标系为：MotionVenus软件3D界面左下角坐标系（右手）
	*/
	float position_meter[3];/*人体骨骼的起始点位置坐标 单位是米*/
	float quat_xyzw[4];/*人体骨骼的旋转四元数，顺序为xyzw 由设置决定是本地坐标系或者全局坐标系*/
	float euler_degree[3];/*人体骨骼的旋转欧拉角，单位是角度 由设置决定是本地坐标系或者全局坐标系*/
	float euler_biasDegree[3];

	/*
	下面六个为一组 输出为佩戴在人体上的传感器原始数据
	坐标系为：现实世界中传感器自身的坐标系（参考磁北），导光柱指向方向为+y 与导光柱右侧垂直为+x 垂直指向传感器正面上方为+z
	*/
	/*传感器原始输出四元数，顺序为xyzw*/
	float sensorQuat_xyzw[4];
	/*传感器原始输出欧拉角，单位是角度*/
	float sensorEuler_degree[3];
	/*传感器原始输出加速度，单位是g*/
	float sensorAccel_g[3];
	/*传感器原始输出线性加速度，单位是g*/
	float sensorLAccel_g[3];
	/*传感器原始输出角速度，单位是degree/second */
	float sensorGyro_DegreePerSecond[3];
	/*传感器原始输出磁力值，单位是毫高斯*/
	float sensorMag_mGauss[3];
}SingleSkeletonDef;

typedef struct {
	/*
	包含当前帧的一些信息
	*/
	StreamHeader hdr;

	/*
	手部32段骨骼的数据，角度、位置等等
	*/
	SingleSkeletonDef skeletons[KHHS32_SkeletonCount];

	/*
	数据来源的IP地址
	例如 192.168.3.21
	*/
	uint8_t remoteIP[4];

	/*
	数据来源的端口号
	*/
	uint32_t remotePort;

	/*
	sdk的版本号信息
	*/
	uint32_t sdkVersion;
}KHHS32PosAttitude_TypeDef;


typedef struct {
	uint8_t actorName[ACTOR_NAME_LEN_MAX];
	int actorNameLen;
	int isNewFramePending;//1: true 0: false
	KHHS32PosAttitude_TypeDef KHHS32PosAttitude;
}GloveSuitManager_TypeDef;
#pragma  pack()
