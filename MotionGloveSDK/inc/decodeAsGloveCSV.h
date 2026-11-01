/**
 * @file  decodeAsGloveCSV.h
 * @brief 手套 CSV 字符流解帧接口（decodeAsGloveCSV_Func）。
 * @ingroup mgsdk_decode
 *
 * @details
 * 模块边界：把 MotionVenus 转发出来的"可读字符串"格式数据流解析为
 * 骨骼姿态结构；实现见 src/decodeAsGloveCSV.cpp。
 * 线程安全：内部使用进程级静态帧管理器（GloveSplitFrameMgr），
 * 同一帧号的多子包需按到达顺序调用，跨数据流并发调用未做互斥保护。
 *
 * 约定：
 *  - 输入帧为 UTF-8 可读字符串，格式 "帧头,数据段"，帧头以空格分词；
 *  - 固定解析 42 段手部骨骼（KHHS42_SkeletonCount），骨骼数量不符返回 -1；
 *  - 帧头缺少 subpackage 字段、字段个数不符、子包不齐时返回 -1；
 *  - 任一指针为 NULL 时返回 -1，不做任何写入。
 */
#pragma once
#include "stdint.h"
#include "include/motionGloveSDKDef.h"

/**
 * @brief 解码返回码：帧头声明 `skeleton_openxr`，布局未定义，整帧丢弃。
 * @details 与参数非法/帧损坏的 -1 区分开，便于调用方记录「收到未知布局」而非「帧有问题」。
 */
#define MGSDK_DECODE_OPENXR_UNSUPPORTED (-2)

/**
 * @brief 解析一行手套 CSV 数据流，把骨骼姿态写入 @p ssd 输出数组。
 *
 * @details
 * 算法：按逗号切出帧头与数据段 → 解析帧头（姿态/顺序/坐标系/帧号/子包索引）→
 * 按帧号与子包索引在内部静态帧管理器中累积数据 → 子包收齐后按 42 段骨骼
 * 逐组解析 position / euler(或 quat)，euler 姿态还会经 EulerToQuat()
 * 换算为四元数一并输出。
 *
 * @param[in]  binStream     一行 CSV 数据流，以 NUL 结尾的 UTF-8 字符串；
 *                           可为 NULL，为 NULL 时返回 -1。
 * @param[out] sh            帧头输出，不可为 NULL；函数开头会以 0 清零后写入。
 * @param[out] ssd           骨骼输出数组，不可为 NULL，容量至少 @p skeletonCount 个
 *                           SingleSkeletonDef；每段骨骼含位置/欧拉角/四元数。
 * @param[in]  skeletonCount 输出数组容量，必须为 ::KHHS32_SkeletonCount（32）或
 *                           ::KHHS42_SkeletonCount（42）；本帧解析出的段数超过该容量时返回 -1。
 *                           调用方传 42 即可同时接收 32 段与 42 段帧。
 *
 * @return 状态码：
 *         - 0  本帧解析成功（子包已收齐且字段校验通过），@p ssd 已填充，
 *              且 @p sh 的 skeletonFormat 字段给出本帧的真实格式；
 *         - -1 参数非法（任一指针为 NULL / 容量既不是 32 也不是 42）、帧头缺少
 *              subpackage 字段、字段个数与声明格式不符、子包尚未收齐、
 *              或帧头出现未知的 skeleton_* 标记；
 *              此时 @p ssd 保持函数开头 memset 后的全 0 状态；
 *         - ::MGSDK_DECODE_OPENXR_UNSUPPORTED 帧头声明 `skeleton_openxr`，
 *              该布局尚未定义，整帧丢弃；此时 @p sh 的 skeletonFormat 已被置为
 *              GloveStreamFormat_OpenXR，@p ssd 不被写入。
 *
 * @note 子包未收齐时返回 -1 是正常流程（等待后续子包），不是错误。
 */
int decodeAsGloveCSV_Func(char* binStream,
	StreamHeader* sh,
	SingleSkeletonDef* ssd,
	unsigned int skeletonCount);