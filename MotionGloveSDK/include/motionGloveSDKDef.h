/**
 * @file  motionGloveSDKDef.h
 * @brief SDK 数据容器定义：单段骨骼、整帧姿态、手套套装管理器。
 * @ingroup mgsdk_types
 *
 * @details
 * 模块边界：类型定义（SingleSkeletonDef / KHHS42PosAttitude_TypeDef /
 * GloveSuitManager_TypeDef），无实现；被 SDK 与示例共用的核心头文件。
 * 数据约定：位置单位米，欧拉角单位度，四元数顺序 xyzw；
 * 传感器数据坐标系为传感器自身坐标系（导光柱方向 +y）。
 */
#pragma once
#include <stdint.h>
#include <cstring>
#include "motionGloveSDK_commonDef.h"
#include "motionGloveSDK_skeleton32Def.h"
#include "motionGloveSDK_skeleton42Def.h"
#include "motionGloveSDK_skeletonOpenXRDef.h"


/** @brief 数据流名称最大长度（字节），含结尾 NUL。 */
#define ACTOR_NAME_LEN_MAX (64)

/** @brief SDK 运行状态：内部线程与 UDP 通道的生命周期状态。 */
typedef enum {
	SDK_Status_None,			///< 初始状态，未做任何操作
	SDK_Status_OpenUDP,			///< 已打开 UDP 端口
	SDK_Status_ThreadRunning,	///< 接收线程运行中
	SDK_Status_ThreadPause		///< 接收线程已暂停

}SDK_Status_TypeDef;

#pragma pack(1)

/**
 * @brief 人体单段骨骼的数据定义，与本段骨骼所有相关的数据都存储在里面。
 * @details 构造函数完成成员初始化：骨骼索引置为无效值，四元数初始化为
 *          单位四元数 (0,0,0,1)，其余数据清零。位置单位米、欧拉角单位度、
 *          四元数顺序 xyzw；传感器数据为佩戴在人体上的传感器原始输出。
 */
typedef struct _SingleSkeleton {
	/** @brief 构造函数：骨骼索引置无效值，四元数置单位四元数，其余数据清零。 */
	_SingleSkeleton()
	{
		gloveSkeletonIndex = (kinemHumanHandsSkeleton42Index)(-1);

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
		kinemHumanHandsSkeleton42Index gloveSkeletonIndex;  ///< 本段骨骼的编号（42 段索引）
		kinemHumanHandsSkeleton32Index gloveSkeletonIndex32;  ///< 本段骨骼的编号（32 段索引；与手套骨格式对应）
	};


	char skeletonName[64];  ///< 骨骼名称（NUL 结尾，最多 63 字节 + 1 结束符）

	/*
	下面三个为一组 输出为人体53段骨骼的位置与旋转信息
	坐标系为：MotionVenus软件3D界面左下角坐标系（右手）
	*/
	float position_meter[3];  ///< 人体骨骼的起始点位置坐标 单位是米
	float quat_xyzw[4];  ///< 人体骨骼的旋转四元数，顺序为xyzw 由设置决定是本地坐标系或者全局坐标系
	float euler_degree[3];  ///< 人体骨骼的旋转欧拉角，单位是角度 由设置决定是本地坐标系或者全局坐标系
	float euler_biasDegree[3];  ///< 人体骨骼的欧拉角偏置，单位是角度

	/*
	下面六个为一组 输出为佩戴在人体上的传感器原始数据
	坐标系为：现实世界中传感器自身的坐标系（参考磁北），导光柱指向方向为+y 与导光柱右侧垂直为+x 垂直指向传感器正面上方为+z
	*/
	float sensorQuat_xyzw[4];  ///< 传感器原始输出四元数，顺序为xyzw
	float sensorEuler_degree[3];  ///< 传感器原始输出欧拉角，单位是角度
	float sensorAccel_g[3];  ///< 传感器原始输出加速度，单位是g
	float sensorLAccel_g[3];  ///< 传感器原始输出线性加速度，单位是g
	float sensorGyro_DegreePerSecond[3];  ///< 传感器原始输出角速度，单位是degree/second
	float sensorMag_mGauss[3];  ///< 传感器原始输出磁力值，单位是毫高斯
}SingleSkeletonDef;

/**
 * @brief 一帧完整的 42 段骨骼数据：帧头 + 骨骼数组 + 数据来源信息。
 * @details hdr 内含协议/姿态/坐标系等帧级信息；
 *          skeletons 为 42 段骨骼的位置、欧拉角与四元数；
 *          remoteIP/remotePort 为数据来源地址，sdkVersion 为 SDK 版本号。
 */
typedef struct {
	StreamHeader hdr;  ///< 当前帧的帧头信息（协议/姿态格式/坐标系等）

	SingleSkeletonDef skeletons[KHHS42_SkeletonCount];  ///< 42 段手部骨骼的数据（角度、位置等）

	uint8_t remoteIP[4];  ///< 数据来源的IP地址，例如 192.168.3.21

	uint32_t remotePort;  ///< 数据来源的端口号

	uint32_t sdkVersion;  ///< SDK 的版本号信息
}KHHS42PosAttitude_TypeDef;

/**
 * @brief 一帧完整的 32 段骨骼数据：帧头 + 骨骼数组 + 数据来源信息。
 * @details 与 #KHHS42PosAttitude_TypeDef 同构，仅骨骼数组长度不同；
 *          骨骼顺序遵循 #kinemHumanHandsSkeleton32Index（右手 16 段 → 左手 16 段）。
 *          独立成类型是为了让 32 段与 42 段各自原生，不做上采样。
 */
typedef struct {
	StreamHeader hdr;  ///< 当前帧的帧头信息（协议/姿态格式/坐标系等）

	SingleSkeletonDef skeletons[KHHS32_SkeletonCount];  ///< 32 段手部骨骼的数据（角度、位置等）

	uint8_t remoteIP[4];  ///< 数据来源的IP地址，例如 192.168.3.21

	uint32_t remotePort;  ///< 数据来源的端口号

	uint32_t sdkVersion;  ///< SDK 的版本号信息
}KHHS32PosAttitude_TypeDef;

/**
 * @brief 一帧 OpenXR 关节数据：帧头 + 关节数组 + 数据来源信息。
 * @details 布局尚未定义，SDK 当前不会收该格式，本类型仅用于冻结公开接口的签名与尺寸；
 *          获取该格式数据的接口恒返回失败，详见 MotionGloveSDK_GetSkeletonsOpenXRFrame()。
 */
typedef struct {
	StreamHeader hdr;  ///< 当前帧的帧头信息（协议/姿态格式/坐标系等）

	SingleSkeletonDef skeletons[KHHSOpenXR_SkeletonCount];  ///< OpenXR 关节数据（布局未定义，当前恒为空）

	uint8_t remoteIP[4];  ///< 数据来源的IP地址，例如 192.168.3.21

	uint32_t remotePort;  ///< 数据来源的端口号

	uint32_t sdkVersion;  ///< SDK 的版本号信息
}KHHSOpenXRPosAttitude_TypeDef;


/**
 * @brief 手套套装管理器：按数据流名称缓存一条数据流的最新帧状态。
 * @details isNewFramePending 标记是否有新帧待取（1 是 / 0 否），取帧后由 SDK 复位；
 *          frameFormat 指明缓存帧的骨骼格式，posAttitude32 / posAttitude42 中
 *          只有与之一致的那个成员有效（另一个为上一次的残留值或全零）。
 *          两个容器并列而非共用 union：SingleSkeletonDef 有用户提供的构造函数，
 *          C++11 下含此类成员的 union 其默认构造函数会被删除，全局数组将无法构造。
 */
typedef struct {
	uint8_t actorName[ACTOR_NAME_LEN_MAX];  ///< 数据流名称（NUL 结尾）
	int actorNameLen;  ///< 数据流名称的有效长度
	int isNewFramePending;  ///< 新帧待取标记：1 是 / 0 否，取帧后由 SDK 复位
	GloveStreamSkeletonFormat_TypeDef frameFormat;  ///< 缓存帧的骨骼格式，决定下面两个容器哪个有效
	KHHS32PosAttitude_TypeDef posAttitude32;  ///< 32 段骨骼的最新一帧；frameFormat 为 GloveStreamFormat_Skeleton32 时有效
	KHHS42PosAttitude_TypeDef posAttitude42;  ///< 42 段骨骼的最新一帧；frameFormat 为 GloveStreamFormat_Skeleton42 时有效
}GloveSuitManager_TypeDef;
#pragma  pack()
