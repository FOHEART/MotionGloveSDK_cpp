/**
 * @file  motionGloveSDK_commonDef.h
 * @brief SDK 通用数据类型：流格式、骨骼姿态/坐标系、旋转顺序、帧头结构。
 * @ingroup mgsdk_types
 *
 * @details
 * 模块边界：纯类型定义（枚举 + StreamHeader 帧头结构，pragma pack(1)），
 * 无实现；被 motionGloveSDKDef.h 等头文件引用。
 * 数据约定：枚举均为一字节（uint8_t）定长，便于二进制流互操作；
 * 欧拉角单位度，位置单位米，四元数顺序 xyzw。
 */
#pragma once
#include <stdint.h>
#include <string>


#pragma pack(1)

/** @brief 设备型号：标识数据来源的动捕设备类型。 */
typedef enum _SuitType :uint8_t
{
	FOHEART_X,		///< FOHEART X 系列动捕设备
	FOHEART_MAGIC,	///< FOHEART Magic 系列动捕设备
	SUITTYPE_UNDEFINE = 255	///< 未定义设备型号
}SuitType;

/** @brief 数据流格式：一帧数据以二进制或可读字符串形式传输。 */
typedef enum _StreamType :uint8_t
{
	StreamFormat_Binary,/*数据流格式为二进制流*/
	StreamFormat_String/*数据流格式为可读字符串*/
}StreamFormat_TypeDef;

/** @brief 骨骼位置格式：数据帧是否包含骨骼位置及其单位。 */
typedef enum _SkeletonPosition :uint8_t
{
	SkeletonPosition_None,	///< 不包含骨骼位置数据
	SkeletonPosition_Meter	///< 包含骨骼位置数据，单位：米
}SkeletonPosition_TypeDef;

/** @brief 传感器姿态格式：数据帧是否包含传感器原始姿态及其表示方法。 */
typedef enum _SensorAttitude :uint8_t
{
	SensorAttitude_None,	///< 不包含传感器姿态数据
	SensorAttitude_Euler,	///< 传感器姿态以欧拉角表示（单位：度）
	SensorAttitude_Quat		///< 传感器姿态以四元数表示（顺序 xyzw）
}SensorAttitude_TypeDef;

/** @brief 传感器加速度格式：数据帧是否包含传感器原始加速度及其单位。 */
typedef enum _SensorAccel :uint8_t
{
	SensorAccel_None,			///< 不包含加速度数据
	SensorAccel_g,				///< 加速度单位：g
	SensorAccel_mPerSquareSecond	///< 加速度单位：m/s^2
}SensorAccel_TypeDef;

/** @brief 传感器线性加速度格式：数据帧是否包含传感器线性加速度及其单位。 */
typedef enum _SensorLAccel :uint8_t
{
	SensorLAccel_None,				///< 不包含线性加速度数据
	SensorLAccel_g,					///< 线性加速度单位：g
	SensorLAccel_mPerSquareSecond	///< 线性加速度单位：m/s^2
}SensorLAccel_TypeDef;

/** @brief 传感器角速度格式：数据帧是否包含传感器原始角速度及其单位。 */
typedef enum _SensorGyro :uint8_t
{
	SensorGyro_None,				///< 不包含角速度数据
	SensorGyro_degreePerSecond,		///< 角速度单位：度/秒
	SensorGyro_radiusPerSecond		///< 角速度单位：弧度/秒
}SensorGyro_TypeDef;

/** @brief 传感器磁力计格式：数据帧是否包含传感器原始磁力值。 */
typedef enum _SensorMag :uint8_t
{
	SensorMag_None,		///< 不包含磁力值数据
	SensorMag_mGauss	///< 磁力值单位：毫高斯（mGauss）
}SensorMag_TypeDef;

/** @brief 手势识别结果：0 表示无手势，1~15 为对应编号的手势。 */
 enum HandGesture_TypeDef : uint8_t
{
	HandGesture_None,	///< 无手势
	HandGesture_1,		///< 手势 1
	HandGesture_2,		///< 手势 2
	HandGesture_3,		///< 手势 3
	HandGesture_4,		///< 手势 4
	HandGesture_5,		///< 手势 5
	HandGesture_6,		///< 手势 6
	HandGesture_7,		///< 手势 7
	HandGesture_8,		///< 手势 8
	HandGesture_9,		///< 手势 9
	HandGesture_10,		///< 手势 10
	HandGesture_11,		///< 手势 11
	HandGesture_12,		///< 手势 12
	HandGesture_13,		///< 手势 13
	HandGesture_14,		///< 手势 14
	HandGesture_15		///< 手势 15
};
/** @brief 手势类型别名：与 #HandGesture_TypeDef 枚举同名的全局别名，保持既有调用方兼容。 */
 typedef HandGesture_TypeDef HandGesture_TypeDef;

/** @brief 骨骼姿态格式：数据帧是否包含骨骼旋转角度及其表示方法。 */
typedef enum _SkeletonAttitude :uint8_t
{
	SkeletonAttitude_None,	///< 不包含骨骼的旋转角度
	SkeletonAttitude_Euler,	///< 骨骼的旋转角度以欧拉角格式表示 单位是Degree
	SkeletonAttitude_Quat	///< 骨骼的旋转角度以四元数格式表示
}SkeletonAttitude_TypeDef;

/** @brief 骨骼坐标系：骨骼旋转角度的参考坐标系。 */
typedef enum _SkeletonCoordinate :uint8_t
{
	SkeletonCoordinate_Relative,	///< 骨骼旋转系为本地坐标系 即当前骨骼相对于其父骨骼的旋转
	SkeletonCoordinate_Global		///< 骨骼旋转系为全局坐标系 即当前骨骼相对于世界坐标系的旋转
}SkeletonCoordinate_TypeDef;

/** @brief 欧拉角旋转顺序：四元数转换为欧拉角时采用的通道顺序（XYZ/XZY/YXZ/YZX/ZXY/ZYX）。 */
typedef enum _MVSDK_ChannelOrder : uint8_t
{
	MVSDK_XYZ,	///< 旋转顺序 XYZ
	MVSDK_XZY,	///< 旋转顺序 XZY
	MVSDK_YXZ,	///< 旋转顺序 YXZ
	MVSDK_YZX,	///< 旋转顺序 YZX
	MVSDK_ZXY,	///< 旋转顺序 ZXY
	MVSDK_ZYX,	///< 旋转顺序 ZYX
	MVSDK_CHANNEL_ORDER_MAX	///< 旋转顺序枚举个数（哨兵值，非合法顺序）
}MVSDK_ChannelOrder;

/** @brief 旋转顺序名称表：索引与 #MVSDK_ChannelOrder 一一对应，用于日志与调试显示。 */
const std::string MVSDK_ChannelOrder_StringDesc[MVSDK_CHANNEL_ORDER_MAX] = {
	"XYZ",
	"XZY",
	"YXZ",
	"YZX",
	"ZXY",
	"ZYX"
};

/**
 * @brief 数据流的骨骼段数格式：由帧头 `fn <帧号>` 之后的可缺省标记决定。
 * @details 帧内没有该标记时为 #GloveStreamFormat_Skeleton32 ，即老格式，
 *          因此该枚举的 0 值刻意与「无标记」等价，结构体清零后即为合法默认值。
 */
typedef enum _GloveStreamSkeletonFormat : uint8_t
{
	GloveStreamFormat_Skeleton32 = 0,	///< 32 段手部骨骼（每手 16 段），帧内无标记时的默认值
	GloveStreamFormat_Skeleton42 = 1,	///< 42 段手部骨骼（每手 21 段，含指尖末端）
	GloveStreamFormat_OpenXR = 2,		///< OpenXR 关节布局，尚未定义，收到即整帧丢弃
	GloveStreamFormat_Count				///< 格式枚举个数（哨兵值，非合法格式）
} GloveStreamSkeletonFormat_TypeDef;

/**
 * @brief 数据流帧头：描述一帧数据的协议版本、演员名称、骨骼配置与姿态格式。
 * @details 字段语义见各成员行尾/行内注释；skeletonPosition / skeletonAttitude /
 *          skeletonCoordinate / channelOrder 与 MotionVenus 软件"转发规则"设置项
 *          一一对应。结构体按 1 字节对齐（pragma pack(1)），可直接网络传输。
 */
typedef struct _stream_header
{
	uint16_t   protocolVersion;  ///< 协议的版本（Version of the protocol）
	uint8_t    AvatarNameLength;  ///< 演员名称的有效长度（应小于64），AvatarName 未用部分填 0x00
	uint8_t    AvatarName[64];  ///< 演员/数据流名称（对应 MotionVenus 设备列表；多名演员通过此项区分数据流）
	uint32_t   suitNumber;  ///< 当前设备的序列号，标签粘贴在接收器底部
	SuitType   suitType;  ///< 设备型号
	uint32_t   frameNumber;  ///< 帧序号，从0开始，默认帧率为每秒100帧
	uint8_t    bodySkeletonCount;  ///< 躯干骨骼数量：默认23；数据手套为2（左手和右手）
	uint8_t    leftFigureSkeletonCount;  ///< 左手指骨的骨骼数量：32 段格式为 15，42 段格式为 20
	uint8_t    rightFigureSkeletonCount;  ///< 右手指骨的骨骼数量：32 段格式为 15，42 段格式为 20
	StreamFormat_TypeDef streamFormat;  ///< 数据流格式（二进制/字符串），对应 转发规则->自定义->数据流
	SkeletonPosition_TypeDef skeletonPosition;  ///< 是否包含53段骨骼位置，对应 转发规则->自定义->人体骨骼->骨骼坐标
	SkeletonAttitude_TypeDef skeletonAttitude;  ///< 是否包含53段骨骼旋转角度及表示方法，对应 转发规则->自定义->人体骨骼->旋转
	SkeletonCoordinate_TypeDef skeletonCoordinate;  ///< 骨骼旋转系（本地/全局），对应 转发规则->自定义->人体骨骼->骨骼旋转系
	MVSDK_ChannelOrder channelOrder;  ///< 欧拉角旋转顺序，对应 转发规则->自定义->旋转顺序
	SensorAttitude_TypeDef sensorAttitude;  ///< 是否包含传感器原始角度及表示方法，对应 转发规则->自定义->传感器->旋转
	SensorAccel_TypeDef sensorAccel;  ///< 是否包含传感器原始加速度，对应 转发规则->自定义->传感器->加速度
	SensorLAccel_TypeDef sensorLAccel;  ///< 是否包含传感器原始线性加速度，对应 转发规则->自定义->传感器->线性加速度
	SensorGyro_TypeDef sensorGyro;  ///< 是否包含传感器原始角速度，对应 转发规则->自定义->传感器->角速度
	SensorMag_TypeDef sensorMag;  ///< 是否包含传感器原始磁力值，对应 转发规则->自定义->传感器->磁力值
#ifdef H1GLOVE_FLEX_VALUE
#if H1GLOVE_FLEX_VALUE
	SensorFlex_TypeDef sensorFlex;  ///< 是否包含原始弯曲传感器值，对应 转发规则->自定义->传感器->弯曲传感器
#endif
#endif // H1GLOVE_FLEX_VALUE
	 HandGesture_TypeDef leftHandGesture;  ///< 左手手势识别结果
	 HandGesture_TypeDef rightHandGesture;  ///< 右手手势识别结果

	GloveStreamSkeletonFormat_TypeDef skeletonFormat;  ///< 本帧骨骼段数格式；帧内无标记时为 GloveStreamFormat_Skeleton32

	  unsigned char resv[39];  ///< 保留字节，当前未使用
#if 0
	/*胯部高度 单位：m*/
	float hipHeight;
	uint8_t unity3D_SkeletonTemplate;
	uint8_t unrealEngine_SkeletonTemplate;
	uint8_t motionBuilder_SkeletonTemplate;
	uint8_t blender_SkeletonTemplate;
	uint8_t c4d_SkeletonTemplate;
	uint8_t otherEngineSkeletonTemplate[5];

	uint8_t idColorRGB[3];

	uint8_t isContainSkeletonEulerBias;//18+24
	float thumb0AddEuler[3];
	float armAddEuler[3];
#endif
}StreamHeader;
/** @brief 流帧头指针别名：指向 #StreamHeader 的指针类型，便于函数传参书写。 */
typedef struct _stream_header *LPStreamHeader;
#pragma  pack()
