/**
 * @file  motionGloveSDK.h
 * @brief 动捕手套 SDK 对外 C 接口：UDP 监听、新帧查询与骨骼姿态获取。
 * @ingroup mgsdk_api
 *
 * @details
 * 模块边界：对外 API 层，全部函数以 extern "C" 导出（MGSDK_DLL_API），
 * 供 C / C++ 调用方使用；内部实现见 src/motionGloveSDK.cpp。
 * 线程安全：网络收发由内部线程完成，查询/获取接口可跨线程调用；
 * 同一数据流的"新帧查询 + 取帧"需按序配对使用。
 *
 * 约定：
 *  - 返回码统一为 0 成功、负值失败；
 *  - 数据流名称 actorName 以 NUL 结尾，长度不超过 63 字节；
 *  - 数据输出结构有两种，按 MotionGloveSDK_GetSkeletonsFrameType() 的返回值选择：
 *    32 段用 KHHS32PosAttitude_TypeDef、42 段用 KHHS42PosAttitude_TypeDef；
 */
#pragma once
#include <stdint.h>
#include <string>
#include "motionGloveSDKDef.h"

/** @brief 数据流名称前缀：手套数据流的帧头以此字符串开头。 */
#define MOTIONGLOVESDK_HEADER         ("MotionGloveSDK ")

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef MGSDK_DLL_EXPORT
        /** @brief 导入/导出宏：编译 SDK 本体时导出符号，调用方使用时导入符号。 */
        #define MGSDK_DLL_API __declspec(dllexport)
    #else
        /** @brief 导入/导出宏：调用方编译时从 DLL 导入符号。 */
        #define MGSDK_DLL_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        /** @brief 导入/导出宏：GCC 4+ 平台以默认可见性导出符号。 */
        #define MGSDK_DLL_API __attribute__ ((visibility("default")))
    #else
        /** @brief 导入/导出宏：其余平台为空，不做导入/导出修饰。 */
        #define MGSDK_DLL_API
    #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the SDK version number.
 * @brief (中文) 获取 SDK 的版本号。
 *
 * @return SDK version as an unsigned integer (e.g. 0x00010000 for v1.0).
 */
MGSDK_DLL_API  unsigned int MotionGloveSDK_getVersion(void);


/**
 * @brief Bind a local UDP port and wait for incoming data.
 * @brief (中文) 绑定一个本机的端口，等待读取数据。
 *
 * @param nPort Port number (1024 ~ 65535), default 5001.
 * @return 0 on success; -1 on failure (commonly port already in use).
 */
 MGSDK_DLL_API int MotionGloveSDK_ListenUDPPort(int nPort);

/**
 * @brief Close the currently opened UDP connection.
 * @brief (中文) 关闭当前打开的UDP连接。
 *
 * @return 0 on success; -1 on failure (commonly port already closed).
 */
 MGSDK_DLL_API int MotionGloveSDK_CloseUDPPort(void);

 /**
  * @brief Check whether a new frame is pending for the given data stream name.
  * @brief (中文) 按照数据流的名称查询是否有新的一帧数据到来。
  *
  * @param actorName Name of the data stream (null-terminated). Max length 64 bytes. Use "0" if single stream.
  * @return true if a new frame is pending; false otherwise.
  */
 MGSDK_DLL_API  bool MotionGloveSDK_isGloveNewFramePending(const char* actorName);

 /**
  * @brief Reset the new-frame pending flag for the given data stream name.
  * @brief (中文) 清除指定数据流的"新帧到来"标记，需与查询接口按序配对使用。
  *
  * @param actorName Name of the data stream (null-terminated). Max length 64 bytes. Use "0" if single stream.
  * @return true if the flag was reset; false otherwise (e.g. stream not found).
  */
 MGSDK_DLL_API  bool MotionGloveSDK_resetGloveNewFramePending(const char* actorName);
 /**
  * @brief Query which skeleton layout the cached frame of a data stream uses.
  * @brief (中文) 查询指定数据流当前缓存帧的骨骼格式（32 段 / 42 段 / OpenXR）。
  *
  * @details 调用方先用本接口得知格式，再选择对应的 GetSkeletons32Frame /
  *          GetSkeletons42Frame 取帧，避免用错接口而拿到错位数据。
  *
  * @param actorName Name of the data stream (null-terminated). Max length 64 bytes.
  * @param pFormat Output pointer receiving the format of the cached frame.
  * @return 0 on success; -1 if actorName is invalid/empty, the stream is unknown,
  *         or pFormat is NULL.
  */
 MGSDK_DLL_API  int  MotionGloveSDK_GetSkeletonsFrameType(const char* actorName, GloveStreamSkeletonFormat_TypeDef* pFormat);

 /**
  * @brief Retrieve the cached 42-segment frame of a data stream.
  * @brief (中文) 获取指定数据流缓存的 42 段骨骼帧（原名 MotionGloveSDK_GetGloveSkeletonsFrame）。
  *
  * @param actorName Name of the data stream (null-terminated).
  * @param pKHHS42PosAttitude Output pointer to receive frame data.
  * @return 0 on success; -1 if the stream is unknown, the cached frame is not in
  *         42-segment layout, or pKHHS42PosAttitude is NULL.
  */
 MGSDK_DLL_API  int  MotionGloveSDK_GetSkeletons42Frame(const char* actorName, KHHS42PosAttitude_TypeDef* pKHHS42PosAttitude);

 /**
  * @brief Retrieve the cached 32-segment frame of a data stream.
  * @brief (中文) 获取指定数据流缓存的 32 段骨骼帧。
  *
  * @param actorName Name of the data stream (null-terminated).
  * @param pKHHS32PosAttitude Output pointer to receive frame data.
  * @return 0 on success; -1 if the stream is unknown, the cached frame is not in
  *         32-segment layout, or pKHHS32PosAttitude is NULL.
  */
 MGSDK_DLL_API  int  MotionGloveSDK_GetSkeletons32Frame(const char* actorName, KHHS32PosAttitude_TypeDef* pKHHS32PosAttitude);

 /**
  * @brief Retrieve the cached OpenXR frame of a data stream.
  * @brief (中文) 获取指定数据流缓存的 OpenXR 关节帧——预留接口。
  *
  * @details OpenXR 布局尚未定义，SDK 当前也收不到该格式的数据，
  *          因此本接口**恒返回 -1**。该签名与容器先行冻结，
  *          使布局确定后只需补齐实现、无需改动 ABI。
  *
  * @param actorName Name of the data stream (null-terminated).
  * @param pKHHSOpenXRPosAttitude Output pointer to receive frame data.
  * @return Always -1 (layout undefined; no such frame can be received yet).
  */
 MGSDK_DLL_API  int  MotionGloveSDK_GetSkeletonsOpenXRFrame(const char* actorName, KHHSOpenXRPosAttitude_TypeDef* pKHHSOpenXRPosAttitude);



#ifdef __cplusplus
};
#endif