#pragma once
#include <stdint.h>
#include <string>


#pragma pack(1)

typedef enum _SuitType :uint8_t
{
	FOHEART_X,
	FOHEART_MAGIC,
	SUITTYPE_UNDEFINE = 255
}SuitType;

/*固定定义为一字节的枚举类型*/
typedef enum _StreamType :uint8_t
{
	StreamFormat_Binary,/*数据流格式为二进制流*/
	StreamFormat_String/*数据流格式为可读字符串*/
}StreamFormat_TypeDef;

/*固定定义为一字节的枚举类型*/
typedef enum _SkeletonPosition :uint8_t
{
	SkeletonPosition_None,
	SkeletonPosition_Meter
}SkeletonPosition_TypeDef;

/*固定定义为一字节的枚举类型*/
typedef enum _SensorAttitude :uint8_t
{
	SensorAttitude_None,
	SensorAttitude_Euler,
	SensorAttitude_Quat
}SensorAttitude_TypeDef;

typedef enum _SensorAccel :uint8_t
{
	SensorAccel_None,
	SensorAccel_g,
	SensorAccel_mPerSquareSecond
}SensorAccel_TypeDef;
/*

*/
typedef enum _SensorLAccel :uint8_t
{
	SensorLAccel_None,
	SensorLAccel_g,
	SensorLAccel_mPerSquareSecond
}SensorLAccel_TypeDef;

typedef enum _SensorGyro :uint8_t
{
	SensorGyro_None,
	SensorGyro_degreePerSecond,
	SensorGyro_radiusPerSecond
}SensorGyro_TypeDef;

typedef enum _SensorMag :uint8_t
{
	SensorMag_None,
	SensorMag_mGauss
}SensorMag_TypeDef;
 enum HandGesture_TypeDef : uint8_t
{
	HandGesture_None,
	HandGesture_1,
	HandGesture_2,
	HandGesture_3,
	HandGesture_4,
	HandGesture_5,
	HandGesture_6,
	HandGesture_7,
	HandGesture_8,
	HandGesture_9,
	HandGesture_10,
	HandGesture_11,
	HandGesture_12,
	HandGesture_13,
	HandGesture_14,
	HandGesture_15
};
 typedef HandGesture_TypeDef HandGesture_TypeDef;

/*固定定义为一字节的枚举类型*/
typedef enum _SkeletonAttitude :uint8_t
{
	SkeletonAttitude_None,/*不包含骨骼的旋转角度*/
	SkeletonAttitude_Euler,/*骨骼的旋转角度以欧拉角格式表示 单位是Degree*/
	SkeletonAttitude_Quat/*骨骼的旋转角度以四元数格式表示*/
}SkeletonAttitude_TypeDef;

/*固定定义为一字节的枚举类型*/
typedef enum _SkeletonCoordinate :uint8_t
{
	SkeletonCoordinate_Relative,/*骨骼旋转系为本地坐标系 即当前骨骼相对于其父骨骼的旋转*/
	SkeletonCoordinate_Global/*骨骼旋转系为全局坐标系 即当前骨骼相对于世界坐标系的旋转*/
}SkeletonCoordinate_TypeDef;

/*固定定义为一字节的枚举类型*/
typedef enum _MVSDK_ChannelOrder : uint8_t
{
	MVSDK_XYZ,
	MVSDK_XZY,
	MVSDK_YXZ,
	MVSDK_YZX,
	MVSDK_ZXY,
	MVSDK_ZYX,
	MVSDK_CHANNEL_ORDER_MAX
}MVSDK_ChannelOrder;

const std::string MVSDK_ChannelOrder_StringDesc[MVSDK_CHANNEL_ORDER_MAX] = {
	"XYZ",
	"XZY",
	"YXZ",
	"YZX",
	"ZXY",
	"ZYX"
};

typedef struct _stream_header
{
	/*
	协议的版本

	Version of the protocol
	*/
	uint16_t   protocolVersion;
	/*
	当前演员/数据流的名称长度，应小于64
	下一项AvatarName的有效名称长度，例如AvatarName为Actor1(Live)，那么它的有效长度为12，数组后部没有使用的部分填入0x00。

	*/
	uint8_t    AvatarNameLength;
	/*
	对应MotionVenus左侧设备列表中演员/数据流名称。
	一名演员穿戴一套动作捕捉系统进行表演，产生一条数据流。
	如果有多名演员同时穿戴动作捕捉系统进行表演，那么将产生多条数据流，不同数据流通过此项进行区分。
	例如：在同一网络端口分别获取到了包含AvatarName为Actor1(Live)和Actor2(Live)的数据帧，那么这代表有两套动捕系统正在使用。

	*/
	uint8_t    AvatarName[64];
	/*
	当前设备的序列号，标签粘贴在接收器底部
	*/
	uint32_t   suitNumber;
	/*
	设备型号
	*/
	SuitType   suitType;
	/*
	帧序号，从0开始，默认帧率为每秒100帧*/
	uint32_t   frameNumber;
	/*
	人体躯干骨骼的数量 默认为23；如果为数据手套，则为2，左手和右手
	*/
	uint8_t    bodySkeletonCount;
	/*
	左手指骨的骨骼数量 默认为15
	*/
	uint8_t    leftFigureSkeletonCount;
	/*
	右手指骨的骨骼数量 默认为15
	*/
	uint8_t    rightFigureSkeletonCount;
	/*
	当前数据流的格式 二进制或字符串 对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->数据流
	*/
	StreamFormat_TypeDef streamFormat;
	/*
	是否包含53段骨骼位置
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->人体骨骼->骨骼坐标
	*/
	SkeletonPosition_TypeDef skeletonPosition;
	/*
	是否包含53段骨骼的旋转角度，以及角度的表示方法
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->人体骨骼->旋转
	*/
	SkeletonAttitude_TypeDef skeletonAttitude;
	/*
	如果包含53段骨骼的旋转角度，规定骨骼旋转系
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->人体骨骼->骨骼旋转系
	*/
	SkeletonCoordinate_TypeDef skeletonCoordinate;
	/*
	如果发送的角度是欧拉角 设定四元数转换为欧拉角时候的旋转顺序
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->旋转顺序
	*/
	MVSDK_ChannelOrder channelOrder;
	/*
	是否包含传感器原始的角度，如果包含，指定角度的表示方法（欧拉角或四元数）
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->旋转
	*/
	SensorAttitude_TypeDef sensorAttitude;
	/*
	是否包含传感器原始的加速度
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->加速度
	*/
	SensorAccel_TypeDef sensorAccel;
	/*
	是否包含传感器原始的线性加速度
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->线性加速度
	*/
	SensorLAccel_TypeDef sensorLAccel;
	/*
	是否包含传感器原始的角速度
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->角速度
	*/
	SensorGyro_TypeDef sensorGyro;
	/*
	是否包含传感器原始的磁力值
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->磁力值
	*/
	SensorMag_TypeDef sensorMag;
#ifdef H1GLOVE_FLEX_VALUE
#if H1GLOVE_FLEX_VALUE
	/*
	是否包含原始的弯曲传感器值
	对应设置为MotionVenus软件工具栏->选项->插件->转发规则->自定义->传感器->弯曲传感器
	*/
	SensorFlex_TypeDef sensorFlex;
#endif
#endif // H1GLOVE_FLEX_VALUE
	/*
	左手手势识别结果
	*/
	 HandGesture_TypeDef leftHandGesture;
	/*
	右手手势识别结果
	*/
	 HandGesture_TypeDef rightHandGesture;

	  unsigned char resv[40];/**/
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
}StreamHeader, *LPStreamHeader;
#pragma  pack()
