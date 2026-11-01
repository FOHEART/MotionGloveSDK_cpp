/**
 * @file  motionGloveSdkHelper.h
 * @brief SDK 内部辅助函数接口：手套套装管理器查找/添加、CSV 帧格式识别。
 * @ingroup mgsdk_decode
 *
 * @details
 * 模块边界：SDK 内部使用的辅助函数，不对外导出（无 MGSDK_DLL_API）；
 * 实现见 src/motionGloveSdkHelper.cpp。
 * 线程安全：纯查询/写入调用方自有数组，无全局状态，是否加锁由调用方决定。
 * 数组约定：套装管理器数组由调用方提供，长度为 @p len；"未占用"以
 *          actorName[0] == 0x00 判定；名称长度上限 ACTOR_NAME_LEN_MAX（64）。
 */
#pragma once
#include <stdint.h>
#include <string>
#include "include/motionGloveSDKDef.h"

/**
 * @brief 按名称在手套套装管理器中搜索，返回首次命中的下标。
 * @details 逐项比较非空槽位的 actorName（memcmp 按 @p nameLen 字节），
 *          空槽位（actorName[0]==0）直接跳过。
 *
 * @param[in] mssm      套装管理器数组首地址，可为 NULL；为 NULL 时返回 -1。
 * @param[in] len       数组元素个数，取值 [1, +inf)；<= 0 时返回 -1。
 * @param[in] actorName 待搜索名称字节串，不可为 NULL；只比较前 @p nameLen 字节。
 * @param[in] nameLen   比较长度，单位字节，取值 [1, ACTOR_NAME_LEN_MAX]；
 *                      超过上限时截断为 ACTOR_NAME_LEN_MAX；<= 0 时返回 -1。
 *
 * @return 命中项下标，取值 [0, len-1]；未找到或参数非法时返回 -1。
 */
int GloveSuitManager_SearchByName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen);

/**
 * @brief 返回套装管理器中第一个未占用槽位的下标。
 *
 * @param[in] mssm 套装管理器数组首地址，不可为 NULL；为 NULL 时行为未定义
 *                 （调用方需先保证非空）。
 * @param[in] len  数组元素个数，取值 [0, +inf)；为 0 时返回 -1。
 *
 * @return 第一个 actorName[0]==0x00 的下标，取值 [0, len-1]；全部占用时返回 -1。
 */
int GloveSuitManager_getFirstUnOccupy(GloveSuitManager_TypeDef* mssm, int len);

/**
 * @brief 把新名称写入套装管理器的第一个空闲槽位。
 * @details 名称按字节复制，不写入 '\0' 结尾符；长度记录在 actorNameLen。
 *
 * @param[in,out] mssm      套装管理器数组首地址，不可为 NULL；成功时写入选中的槽位。
 * @param[in]     len       数组元素个数，取值 [1, +inf)；<= 0 或全满时返回 -1。
 * @param[in]     actorName 待写入名称字节串，不可为 NULL；只复制前 @p nameLen 字节。
 * @param[in]     nameLen   复制长度，单位字节，取值 [1, ACTOR_NAME_LEN_MAX]；
 *                          超过上限时截断；<= 0 时返回 -1。
 *
 * @return 写入成功的槽位下标，取值 [0, len-1]；无空闲槽位或参数非法时返回 -1。
 */
int GloveSuitManager_AddName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen);

/**
 * @brief 判断字符串是否为 CSV 字符流（以 "Glove" 或 "Actor" 开头即视为 CSV）。
 *
 * @param[in] buff 待判定字符串，可包含任意前缀；可为 NULL，为 NULL 时返回 -1。
 *
 * @return 0 表示是 CSV 格式；-1 表示不是（含空指针）。
 */
int isFrameCSVFormat(const char* buff);

/**
 * @brief 判断字符串是否为手套 CSV 流（必须以 "Glove" 开头）。
 *
 * @param[in] buff 待判定字符串，不可为 NULL；为 NULL 时返回 -1。
 *
 * @return 0 表示是手套 CSV 流；-1 表示不是（含空指针、非开头出现 Glove）。
 */
int isFrameCSV_Glove(const char* buff);

/**
 * @brief 判断字符串是否为全身动捕 CSV 流（必须以 "Actor" 开头）。
 *
 * @param[in] buff 待判定字符串，不可为 NULL；为 NULL 时返回 -1。
 *
 * @return 0 表示是全身动捕 CSV 流；-1 表示不是（含空指针、非开头出现 Actor）。
 */
int isFrameCSV_FullBodyMocap(const char* buff);