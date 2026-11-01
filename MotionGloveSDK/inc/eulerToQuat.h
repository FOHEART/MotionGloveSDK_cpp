/**
 * @file  eulerToQuat.h
 * @brief 欧拉角转四元数接口（EulerToQuat）。
 * @ingroup mgsdk_math
 *
 * @details
 * 模块边界：纯计算，仅供 MotionGloveSDK 内部解帧链路与单元测试使用，
 * 对外请使用 MotionGloveSDK_GetSkeletons42Frame() 获取骨骼姿态。
 * 线程安全：纯函数，无全局状态，可并发调用。
 *
 * 约定：
 *  - 输入角度单位一律为度（degree），内部换算为弧度；
 *  - 输入含 NaN / Inf 时拒绝计算并保持 @p fData 原值，不写入任何非有限值；
 *  - @p RotOrder 越界时输出单位四元数（安全中性值），不读取未初始化内存；
 *  - 结果写入顺序为 x y z w，与骨骼数据定义 SingleSkeletonDef::quat_xyzw 一致。
 */
#pragma once
#include "include/motionGloveSDKDef.h"

/**
 * @brief 把欧拉角换算为四元数，结果按 x,y,z,w 顺序写入 @p fData。
 *
 * @details
 * 算法：按 @p RotOrder 组装旋转矩阵后，用 Shepperd 方法（最大迹分支）
 * 提取四元数并归一化；欧拉角合成为旋转矩阵、再由矩阵取四元数，避免了
 * 固定轴公式在特定角度下的万向节退化。
 *
 * @param[in]  x        绕 X 轴旋转角，单位度，取值 (-inf, +inf)；
 *                      非有限值（NaN/Inf）时拒绝计算。
 * @param[in]  y        绕 Y 轴旋转角，单位度，取值 (-inf, +inf)；
 *                      非有限值（NaN/Inf）时拒绝计算。
 * @param[in]  z        绕 Z 轴旋转角，单位度，取值 (-inf, +inf)；
 *                      非有限值（NaN/Inf）时拒绝计算。
 * @param[in]  RotOrder 旋转顺序枚举，取值必须是 ::MVSDK_ChannelOrder 中的
 *                      合法项（MVSDK_XYZ ~ MVSDK_ZYX）；越界时输出单位四元数。
 * @param[out] fData    输出四元数缓冲区，容量至少 4 个 float；
 *                      成功时写入顺序为 x,y,z,w，模长为 1；
 *                      @p fData 为 NULL 时函数直接返回，不做任何写入。
 *
 * @note 非法输入（NULL / 非有限角度 / 越界顺序）都不会产生崩溃或未定义行为。
 */
void EulerToQuat(float x, float y, float z, MVSDK_ChannelOrder RotOrder, float* fData);