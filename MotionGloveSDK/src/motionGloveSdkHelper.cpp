/**
 * @file  motionGloveSdkHelper.cpp
 * @brief SDK 内部辅助函数实现：套装管理器查找/添加、CSV 帧格式识别。
 * @ingroup mgsdk_decode
 *
 * @details
 * 本文件实现 inc/motionGloveSdkHelper.h 声明的全部辅助函数；
 * 参数与返回值语义以头文件声明处为准（不在此重复文档，避免漂移）。
 */
#include "inc/motionGloveSdkHelper.h"

// 名称比较长度超过上限时截断；名称长度 <= 0、数组指针为空、长度 <= 0
// 都视为参数非法，直接返回 -1。
int GloveSuitManager_SearchByName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen)
{
	int ret = -1;
	if (nameLen > ACTOR_NAME_LEN_MAX)
	{
		nameLen = ACTOR_NAME_LEN_MAX;
	}

	if (nameLen <= 0)
	{
		return ret;
	}
	if (mssm == 0)
	{
		return ret;
	}
	if (len <= 0)
	{
		return ret;
	}


	for (int i = 0; i < len; i++)
	{
		if (mssm[i].actorName[0] == 0x00)
		{
			/*跳过为0的*/
			continue;
		}
		else
		{
			if (0 == memcmp(actorName, mssm[i].actorName, nameLen))
			{
				ret = i;
				break;
			}
		}
	}

	return ret;
}


// "未占用"以 actorName[0] == 0x00 判定，找到第一个即返回。
int GloveSuitManager_getFirstUnOccupy(GloveSuitManager_TypeDef* mssm, int len)
{
	int ret = -1;


	for (int i = 0; i < len; i++)
	{
		if (mssm[i].actorName[0] == 0x00)
		{
			ret = i;
			/*跳过为0的*/
			break;
		}

	}

	return ret;
}


// 先找空闲槽位，再按字节复制名称（不写 NUL 结尾符），长度记入 actorNameLen。
int GloveSuitManager_AddName(GloveSuitManager_TypeDef* mssm, int len, uint8_t* actorName, int nameLen)
{
	int ret = -1;
	if (nameLen > ACTOR_NAME_LEN_MAX)
	{
		nameLen = ACTOR_NAME_LEN_MAX;
	}

	if (nameLen <= 0)
	{
		return ret;
	}

	int unOccupyIdx = GloveSuitManager_getFirstUnOccupy(mssm, len);
	if (unOccupyIdx < 0)
		return ret;
	for (int i = 0; i < nameLen; i++)
	{
		mssm[unOccupyIdx].actorName[i] = actorName[i];
	}
	mssm[unOccupyIdx].actorNameLen = nameLen;
	return unOccupyIdx;
}

// "Glove" 必须出现在字符串开头（下标 0），出现在其他位置不算命中。
int isFrameCSV_Glove(const char* buff)
{
	int ret = -1;
	if (buff == 0)
		return ret;

	std::string buffStr(buff);
	size_t foundGlove = buffStr.find("Glove");
	if (foundGlove != std::string::npos)
	{
		if (foundGlove == 0)
		{
			ret = 0;

			return ret;
		}
		//found
	}
	else
	{
		//not found
	}
	return ret;
}
// "Actor" 必须出现在字符串开头（下标 0），用于区分全身动捕数据流。
int isFrameCSV_FullBodyMocap(const char* buff)
{
	int ret = -1;
	if (buff == 0)
		return ret;

	//----
	std::string buffStr(buff);
	size_t foundFullBodyMocap = buffStr.find("Actor");
	if (foundFullBodyMocap != std::string::npos)
	{
		if (foundFullBodyMocap == 0)
		{
			ret = 0;

			return ret;
		}
		//found
	}
	else
	{
		//not found
	}
	return ret;
}
// CSV 判定 = 手套流或全身流任一命中。
int isFrameCSVFormat(const char* buff)
{
	int ret = -1;
	if (buff == 0)
		return ret;

	if ((0 == isFrameCSV_Glove(buff))||
		(0 == isFrameCSV_FullBodyMocap(buff))
		)
	{
		ret = 0;
		return ret;
	}

	return ret;
	
}