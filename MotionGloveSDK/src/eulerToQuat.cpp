#include "math.h"
#include "inc/eulerToQuat.h"

const float PI = 3.14159265358979323846f;


void rotation2quat(float* q, float* rotmat)
{
	float T = 1 + rotmat[0] + rotmat[4] + rotmat[8];

	// Calculate quaternion based on largest diagonal element
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

	//Normalize the quaternion
	T = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

	q[0] = q[0] / T;
	q[1] = q[1] / T;
	q[2] = q[2] / T;
	q[3] = q[3] / T;
}
/*
fDataË³Ðò: x y z w
*/
void EulerToQuat(float x, float y, float z, MVSDK_ChannelOrder RotOrder, float* fData)
{
	float RotMatrix[9];
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
		break;
	}

	fData[0] = 0;
	fData[1] = 0;
	fData[2] = 0;
	fData[3] = 1;

	rotation2quat(fData, RotMatrix);
}
