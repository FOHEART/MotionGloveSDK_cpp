#pragma once
#include <stdint.h>
#include <string>
#include "motionGloveSDKDef.h"

#define MOTIONGLOVESDK_HEADER         ("MotionGloveSDK ")

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef MGSDK_DLL_EXPORT
        #define MGSDK_DLL_API __declspec(dllexport)
    #else
        #define MGSDK_DLL_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define MGSDK_DLL_API __attribute__ ((visibility("default")))
    #else
        #define MGSDK_DLL_API
    #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

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
 MGSDK_DLL_API  bool MotionGloveSDK_resetGloveNewFramePending(const char* actorName);
 /**
  * @brief Retrieve the actual data when a new pose/frame is pending.
  * @brief (中文) 如果调用MotionGloveSDK_isNewPosePending返回为true，则再次执行这个函数获取实际的数据内容。
  *
  * @param actorName Name of the data stream (null-terminated).
  * @param pKHHS32PosAttitude Output pointer to receive frame data.
  * @return 0 on success; -1 on failure (e.g., stream not found or actorName not null-terminated).
  */
 MGSDK_DLL_API  int  MotionGloveSDK_GetGloveSkeletonsFrame(const char* actorName, KHHS32PosAttitude_TypeDef* pKHHS32PosAttitude);



#ifdef __cplusplus
};
#endif