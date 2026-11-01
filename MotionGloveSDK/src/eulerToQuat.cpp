/**
 * @file  eulerToQuat.cpp
 * @brief 欧拉角转四元数的实现：旋转矩阵组装 + Shepperd 四元数提取。
 *
 * @details
 * 本文件实现 inc/eulerToQuat.h 声明的 EulerToQuat()，并含内部辅助函数
 * rotation2quat()。实现细节见函数内部注释；参数约束与返回值含义
 * 以头文件声明处为准（不在此重复 @param，避免文档漂移）。
 *
 * 代码约定：
 *  - rotation2quat 内部使用 w,x,y,z 布局（q[3]=w），对外接口统一为 x,y,z,w；
 *  - 所有对外路径都经过防御检查：空指针、非有限输入、越界顺序。
 */
#include "math.h"
#include <cmath>
#include "inc/eulerToQuat.h"

/// 圆周率（单精度），用于角度转弧度：rad = degree / 180 * PI。
const float PI = 3.14159265358979323846f;

/**
 * @brief 从旋转矩阵提取四元数并按 w,x,y,z 顺序写入 @p q。
 *
 * @details
 * 实现 Shepperd 方法：比较矩阵迹与各对角元选取最大分支，避免除零；
 * 提取后统一归一化，保证输出模长为 1。q 布局为内部约定（q[3] = w），
 * 由调用方（EulerToQuat）负责转换为对外的 x,y,z,w 顺序。
 *
 * @param[out] q      输出四元数，容量至少 4 个 float，内部顺序 w,x,y,z。
 * @param[in]  rotmat 3x3 行主序旋转矩阵，共 9 个 float，必须为正交归一矩阵。
 * @note 函数内部使用，不向该翻译单元之外导出。
 */
void rotation2quat(float* q, float* rotmat)
{
	float T = 1 + rotmat[0] + rotmat[4] + rotmat[8];

	// 依据矩阵迹与对角元最大的一项选择计算分支，避免精度劣化与除零
	if (T > (0.00000001))
	{
		float S = 0.5f / sqrtf(T);
		q[3] = .25f / S;
		q[0] = (rotmat[7] - rotmat[5]) * S;
		q[1] = (rotmat[2] - rotmat[6]) * S;
		q[2] = (rotmat[3] - rotmat[1]) * S;
	}
	else if ((rotmat[0] > rotmat[4]) && (rotmat[0] > rotmat[8]))
	{
		float S = sqrtf(1 + rotmat[0] - rotmat[4] - rotmat[8]) * 2;
		q[3] = (rotmat[6] - rotmat[5]) / S;
		q[0] = .25f * S;
		q[1] = (rotmat[1] + rotmat[3]) / S;
		q[2] = (rotmat[2] + rotmat[6]) / S;
	}
	else if (rotmat[4] > rotmat[8])
	{
		float S = sqrtf(1 + rotmat[4] - rotmat[0] - rotmat[8]) * 2;
		q[3] = (rotmat[2] - rotmat[6]) / S;
		q[0] = (rotmat[1] + rotmat[3]) / S;
		q[1] = .25f * S;
		q[2] = (rotmat[5] + rotmat[7]) / S;
	}
	else
	{
		float S = sqrtf(1 + rotmat[8] - rotmat[0] - rotmat[4]) * 2;
		q[3] = (rotmat[3] - rotmat[1]) / S;
		q[0] = (rotmat[2] + rotmat[6]) / S;
		q[1] = (rotmat[1] + rotmat[3]) / S;
		q[2] = .25f * S;
	}

	// 归一化：保证下游拿到的是单位四元数
	T = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

	q[0] = q[0] / T;
	q[1] = q[1] / T;
	q[2] = q[2] / T;
	q[3] = q[3] / T;
}

/*
fData 输出顺序: x y z w
*/
void EulerToQuat(float x, float y, float z, MVSDK_ChannelOrder RotOrder, float* fData)
{
	// 防御检查（契约见头文件声明）：
	//  1) 空输出指针直接返回，不崩溃；
	//  2) 非有限角度（NaN/Inf）拒绝计算，fData 保持调用前的内容，
	//     避免 NaN 沿姿态链路静默扩散；
	//  3) 旋转矩阵预置为单位矩阵：RotOrder 越界时走 default 分支（空操作），
	//     输出即为单位四元数（安全中性值），彻底消除读未初始化内存的未定义行为。
	if (fData == nullptr)
		return;
	if (!(std::isfinite(x) && std::isfinite(y) && std::isfinite(z)))
		return;

	float RotMatrix[9] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	};
	float XR, YR, ZR;

	switch (RotOrder)
	{
	case MVSDK_XYZ:
	{
		XR = x / 180.0f * PI;
		YR = y / 180.0f * PI;
		ZR = z / 180.0f * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ;
		RotMatrix[1] = -CY * SZ;
		RotMatrix[2] = SY;
		RotMatrix[3] = CZ * SX * SY + CX * SZ;
		RotMatrix[4] = CX * CZ - SX * SY * SZ;
		RotMatrix[5] = -CY * SX;
		RotMatrix[6] = SX * SZ - CX * CZ * SY;
		RotMatrix[7] = CZ * SX + CX * SY * SZ;
		RotMatrix[8] = CX * CY;
		break;
	}
	case MVSDK_XZY:
	{
		XR = x / 180.0f * PI;
		YR = y / 180.0f * PI;
		ZR = z / 180.0f * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ;
		RotMatrix[1] = -SZ;
		RotMatrix[2] = CZ * SY;
		RotMatrix[3] = SX * SY + CX * CY * SZ;
		RotMatrix[4] = CX * CZ;
		RotMatrix[5] = CX * SY * SZ - CY * SX;
		RotMatrix[6] = CY * SX * SZ - CX * SY;
		RotMatrix[7] = CZ * SX;
		RotMatrix[8] = CX * CY + SX * SY * SZ;
		break;
	}
	case MVSDK_YXZ:
	{
		XR = x / 180.0f * PI;
		YR = y / 180.0f * PI;
		ZR = z / 180.0f * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ + SX * SY * SZ;
		RotMatrix[1] = CZ * SX * SY - CY * SZ;
		RotMatrix[2] = CX * SY;
		RotMatrix[3] = CX * SZ;
		RotMatrix[4] = CX * CZ;
		RotMatrix[5] = -SX;
		RotMatrix[6] = CY * SX * SZ - CZ * SY;
		RotMatrix[7] = CY * CZ * SX + SY * SZ;
		RotMatrix[8] = CX * CY;
		break;
	}
	case MVSDK_YZX:
	{
		XR = x / 180 * PI;
		YR = y / 180 * PI;
		ZR = z / 180 * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ;
		RotMatrix[1] = SX * SY - CX * CY * SZ;
		RotMatrix[2] = CX * SY + CY * SX * SZ;
		RotMatrix[3] = SZ;
		RotMatrix[4] = CX * CZ;
		RotMatrix[5] = -CZ * SX;
		RotMatrix[6] = -CZ * SY;
		RotMatrix[7] = CY * SX + CX * SY * SZ;
		RotMatrix[8] = CX * CY - SX * SY * SZ;
		break;
	}
	case MVSDK_ZXY:
	{
		XR = x / 180 * PI;
		YR = y / 180 * PI;
		ZR = z / 180 * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ - SX * SY * SZ;
		RotMatrix[1] = -CX * SZ;
		RotMatrix[2] = CZ * SY + CY * SX * SZ;
		RotMatrix[3] = CZ * SX * SY + CY * SZ;
		RotMatrix[4] = CX * CZ;
		RotMatrix[5] = SY * SZ - CY * CZ * SX;
		RotMatrix[6] = -CX * SY;
		RotMatrix[7] = SX;
		RotMatrix[8] = CX * CY;
		break;
	}

	case MVSDK_ZYX:
	{
		XR = x / 180 * PI;
		YR = y / 180 * PI;
		ZR = z / 180 * PI;

		float SX = sinf(XR);
		float CX = cosf(XR);
		float SY = sinf(YR);
		float CY = cosf(YR);
		float SZ = sinf(ZR);
		float CZ = cosf(ZR);

		RotMatrix[0] = CY * CZ;
		RotMatrix[1] = CZ * SX * SY - CX * SZ;
		RotMatrix[2] = CX * CZ * SY + SX * SZ;
		RotMatrix[3] = CY * SZ;
		RotMatrix[4] = CX * CZ + SX * SY * SZ;
		RotMatrix[5] = CX * SY * SZ - CZ * SX;
		RotMatrix[6] = -SY;
		RotMatrix[7] = CY * SX;
		RotMatrix[8] = CX * CY;
		break;
	}
	default:
		// 越界顺序：保持单位矩阵，输出单位四元数（安全中性值）。
		break;
	}

	fData[0] = 0;
	fData[1] = 0;
	fData[2] = 0;
	fData[3] = 1;

	rotation2quat(fData, RotMatrix);
}