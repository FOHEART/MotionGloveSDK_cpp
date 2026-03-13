#include "inc/motionGloveSdkHelper.h"

/**
 * @brief Search suit manager by actor name.
 * @brief (中文) 根据名称在手套套装管理器中搜索。
 *
 * @param mssm Pointer to suit manager array.
 * @param len Length of the array.
 * @param actorName Name to search for.
 * @param nameLen Length of the name to compare.
 * @return index if found; -1 if not found.
 */
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


/**
 * @brief Find first unoccupied slot in suit manager.
 * @brief (中文) 获取第一个未占用的套装索引。
 *
 * @param mssm Pointer to suit manager array.
 * @param len Length of the array.
 * @return index of first unoccupied slot or -1 if none.
 */
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


/**
 * @brief Add an actor name to the suit manager.
 * @brief (中文) 向套装管理器中添加名称。
 *
 * @param mssm Pointer to suit manager array.
 * @param len Length of the array.
 * @param actorName Name bytes to copy.
 * @param nameLen Number of bytes in name.
 * @return index of the added suit or -1 on failure.
 */
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
/**
 * @brief Check whether the string is CSV format (starts with "Glove" or "Actor").
 * @brief (中文) 检查字符串是否是csv格式的，特征是字符串以 Glove 或者 Actor 开头。
 *
 * @param buff Input string buffer.
 * @return 0 if CSV format; -1 otherwise.
 */
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