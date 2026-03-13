#include "inc/decodeAsGloveCSV.h"
#include <vector>
#include <algorithm>
#include "inc/eulerToQuat.h"

class GloveSplitFrameMgr
{
public:
	GloveSplitFrameMgr(){ reset(); }
	~GloveSplitFrameMgr(){}
	void reset()
	{
		m_name = "";
		m_data = "";
		fn = 0;
		subFrameVec.clear();
		subFrameMax = 0;

		skeletonPosition = SkeletonPosition_None;
		skeletonAttitude = SkeletonAttitude_Euler;
		channelOrder = MVSDK_ZXY;



		for (int i = 0; i < KHHS32_SkeletonCount; i++)
		{
			pos[i][0] = 0;
			pos[i][1] = 0; 
			pos[i][2] = 0;
		}
		for (int i = 0; i < KHHS32_SkeletonCount; i++)
		{
			euler[i][0] = 0;
			euler[i][1] = 0;
			euler[i][2] = 0;
		}
		for (int i = 0; i < KHHS32_SkeletonCount; i++)
		{
			quat[i][0] = 0;
			quat[i][1] = 0;
			quat[i][2] = 0;
			quat[i][3] = 0;
		}
	}

	std::vector<std::string> split(std::string str, std::string pattern)
	{
		std::string::size_type pos;
		std::vector<std::string> result;
		str += pattern;//扩展字符串以方便操作
		unsigned int size = str.size();
		for (unsigned int i = 0; i < size; i++)
		{
			pos = str.find(pattern, i);
			if (pos < size)
			{
				std::string s = str.substr(i, pos - i);
				if (!s.empty())
					result.push_back(s);
				i = pos + pattern.size() - 1;
			}
		}
		return result;

	}
	bool checkFrameComplete()
	{
		if (subFrameVec.size() == subFrameMax)
		{
			//printf("%s\r\n", m_data.c_str());
			return true;
		}

		return false;
	}

	bool parseFrame()
	{
		std::vector<std::string> dataVec;
		dataVec = split(m_data, ",");
		unsigned  int groupItemCount = 0;

		if (skeletonPosition == SkeletonPosition_Meter)
		{
			groupItemCount += 3;
		}
		if (skeletonAttitude == SkeletonAttitude_Euler)
		{
			groupItemCount += 3;
		}
		else if (skeletonAttitude == SkeletonAttitude_Quat)
		{
			groupItemCount += 4;
		}

		//检查总个数
		if (dataVec.size() == groupItemCount*KHHS32_SkeletonCount)
		{

		}
		else
		{
			printf("[err]");
			return false;
		}

		for (int skeletonIndex = 0; skeletonIndex < KHHS32_SkeletonCount; skeletonIndex++)
		{
			//for (int groupIndex = 0; groupIndex < groupItemCount; groupItemCount++)
			{
				unsigned int groupStartIndex = skeletonIndex*groupItemCount;
				if (skeletonPosition == SkeletonPosition_Meter)
				{
					pos[skeletonIndex][0] = std::stof(dataVec[groupStartIndex + 0]);
					pos[skeletonIndex][1] = std::stof(dataVec[groupStartIndex + 1]);
					pos[skeletonIndex][2] = std::stof(dataVec[groupStartIndex + 2]);
				}
				
				if (skeletonAttitude == SkeletonAttitude_Euler)
				{
					euler[skeletonIndex][0] = std::stof(dataVec[groupStartIndex + 3]);
					euler[skeletonIndex][1] = std::stof(dataVec[groupStartIndex + 4]);
					euler[skeletonIndex][2] = std::stof(dataVec[groupStartIndex + 5]);
				}
				else if (skeletonAttitude == SkeletonAttitude_Quat)
				{
					quat[skeletonIndex][0] = std::stof(dataVec[groupStartIndex + 3]);
					quat[skeletonIndex][1] = std::stof(dataVec[groupStartIndex + 4]);
					quat[skeletonIndex][2] = std::stof(dataVec[groupStartIndex + 5]);
					quat[skeletonIndex][3] = std::stof(dataVec[groupStartIndex + 6]);
				
				}
			}
		}

		if (skeletonAttitude == SkeletonAttitude_Euler)
		{
			for (int skeletonIndex = 0; skeletonIndex < KHHS32_SkeletonCount; skeletonIndex++)
			{
				float tempQuat_xyzw[4] = { 0, 0, 0, 1 };
				EulerToQuat(
					euler[skeletonIndex][0],
					euler[skeletonIndex][1],
					euler[skeletonIndex][2],
					channelOrder,
					tempQuat_xyzw
					);
				quat[skeletonIndex][0] = tempQuat_xyzw[3];
				quat[skeletonIndex][1] = tempQuat_xyzw[0];
				quat[skeletonIndex][2] = tempQuat_xyzw[1];
				quat[skeletonIndex][3] = tempQuat_xyzw[2];
			}

		}

		return true;
	}

	SkeletonPosition_TypeDef skeletonPosition;
	SkeletonAttitude_TypeDef skeletonAttitude;
	MVSDK_ChannelOrder channelOrder;

	std::string m_name;
	//m_data拼完整再解析
	std::string m_data;

	unsigned int fn;
	std::vector<unsigned int> subFrameVec;
	unsigned int subFrameMax;
	float pos[KHHS32_SkeletonCount][3];
	float euler[KHHS32_SkeletonCount][3];
	float quat[KHHS32_SkeletonCount][4];
};
GloveSplitFrameMgr m_GloveSplitFrameMgr;

/*
0x011fa400 "Glove1-SN-1403423E pos euler ZXY relative fn 340772  subpackage 1/1
*/
int decodeAsGloveCSV_Func(char* binStream, 
	StreamHeader* sh, 
	SingleSkeletonDef* ssd, 
	unsigned int skeletonCount)
{
	int ret = -1;
	int index = 0;

	if (nullptr == binStream)
		return -1;
	if (nullptr == sh)
		return -1;
	if (nullptr == ssd)
		return -1;
	if (32 != skeletonCount)
		return -1;
	/*清空容器*/
	memset((void*)sh, 0x00, sizeof(StreamHeader));
	for (unsigned int i = 0; i < skeletonCount; i++)
	{
		memset((void*)&ssd[i], 0x00, sizeof(SingleSkeletonDef));
	}

	std::string binStreamStr(binStream);

	std::vector<std::string> splitBinStreamStrVec = m_GloveSplitFrameMgr.split(binStreamStr, ",");

	std::string header = splitBinStreamStrVec[0];
	std::vector<std::string> splitHeaderVec = m_GloveSplitFrameMgr.split(header, " ");

	sh->AvatarNameLength = splitHeaderVec[0].length();

	strcpy((char*)(sh->AvatarName), splitHeaderVec[0].c_str());

	sh->bodySkeletonCount = 2;
	sh->leftFigureSkeletonCount = 15;
	sh->rightFigureSkeletonCount = 15;

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "pos"))
	{
		sh->skeletonPosition = SkeletonPosition_Meter;
	}

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "quat"))
	{
		sh->skeletonAttitude = SkeletonAttitude_Quat;
	}
	else if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "euler"))
	{
		sh->skeletonAttitude = SkeletonAttitude_Euler;
		auto it = std::find(splitHeaderVec.begin(), splitHeaderVec.end(), "euler");
		int dis = std::distance(splitHeaderVec.begin(), it);

		int rotOrderDis = dis + 1;

		for (int i = 0; i < MVSDK_CHANNEL_ORDER_MAX; i++)
		{
			if (MVSDK_ChannelOrder_StringDesc[i] == splitHeaderVec[rotOrderDis])
			{
				sh->channelOrder = (MVSDK_ChannelOrder)i;
				break;
			}
		}
	}

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "relative"))
	{
		sh->skeletonCoordinate = SkeletonCoordinate_Relative;
	}else if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "global"))
	{
		sh->skeletonCoordinate = SkeletonCoordinate_Global;
	}

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "fn"))
	{
		auto it = std::find(splitHeaderVec.begin(), splitHeaderVec.end(), "fn");
		int dis = std::distance(splitHeaderVec.begin(), it);

		int fnDis = dis + 1;

		int fnConverted = std::stoi(splitHeaderVec[fnDis], 0, 10);
		sh->frameNumber = fnConverted;
	}

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "gesture"))
	{
		auto it = std::find(splitHeaderVec.begin(), splitHeaderVec.end(), "gesture");
		int dis = std::distance(splitHeaderVec.begin(), it);

		int lefthandDis = dis + 1;
		int lefthandConverted = std::stoi(splitHeaderVec[lefthandDis], 0, 10);
		sh->leftHandGesture = (HandGesture_TypeDef)lefthandConverted;

		int righthandDis = dis + 2;
		int righthandConverted = std::stoi(splitHeaderVec[righthandDis], 0, 10);
		sh->rightHandGesture = (HandGesture_TypeDef)righthandConverted;
	}

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "subpackage"))
	{
		auto it = std::find(splitHeaderVec.begin(), splitHeaderVec.end(), "subpackage");
		int dis = std::distance(splitHeaderVec.begin(), it);

		int fnDis = dis + 1;

		std::string subpakPer = splitHeaderVec[fnDis];

		std::vector<std::string> splitSubpakPerVec = m_GloveSplitFrameMgr.split(subpakPer, "/");

		int subPak1 = std::stoi(splitSubpakPerVec[0], 0, 10);
		int subPak2 = std::stoi(splitSubpakPerVec[1], 0, 10);

		if (m_GloveSplitFrameMgr.fn!= sh->frameNumber)
		{
			//new frame
			m_GloveSplitFrameMgr.reset();

			m_GloveSplitFrameMgr.channelOrder = sh->channelOrder;
			m_GloveSplitFrameMgr.skeletonPosition = sh->skeletonPosition;
			m_GloveSplitFrameMgr.skeletonAttitude = sh->skeletonAttitude;


			m_GloveSplitFrameMgr.fn = sh->frameNumber;
			m_GloveSplitFrameMgr.subFrameVec.push_back(subPak1);
			m_GloveSplitFrameMgr.subFrameMax = subPak2;

			std::string s = binStreamStr.substr(header.size() + 1);
			m_GloveSplitFrameMgr.m_data += s;
		}
		else
		{
			//add to current frame
			m_GloveSplitFrameMgr.subFrameVec.push_back(subPak1);
			std::string s = binStreamStr.substr(header.size() + 1);
			m_GloveSplitFrameMgr.m_data += s;
		}
		bool isComplete = m_GloveSplitFrameMgr.checkFrameComplete();

		if (isComplete&&m_GloveSplitFrameMgr.parseFrame())
		{
			for (int skeletonIndex = 0; skeletonIndex < KHHS32_SkeletonCount; skeletonIndex++)
			{
				ssd[skeletonIndex].gloveSkeletonIndex = (kinemHumanHandsSkeleton32Index)(skeletonIndex);

				kinemHumanHandsSkeleton32[skeletonIndex].copy(
					ssd[skeletonIndex].skeletonName,
					kinemHumanHandsSkeleton32[skeletonIndex].length(),
					0);

				ssd[skeletonIndex].position_meter[0] = m_GloveSplitFrameMgr.pos[skeletonIndex][0];
				ssd[skeletonIndex].position_meter[1] = m_GloveSplitFrameMgr.pos[skeletonIndex][1];
				ssd[skeletonIndex].position_meter[2] = m_GloveSplitFrameMgr.pos[skeletonIndex][2];

				ssd[skeletonIndex].euler_degree[0] = m_GloveSplitFrameMgr.euler[skeletonIndex][0];
				ssd[skeletonIndex].euler_degree[1] = m_GloveSplitFrameMgr.euler[skeletonIndex][1];
				ssd[skeletonIndex].euler_degree[2] = m_GloveSplitFrameMgr.euler[skeletonIndex][2];


				ssd[skeletonIndex].quat_xyzw[0] = m_GloveSplitFrameMgr.quat[skeletonIndex][1];
				ssd[skeletonIndex].quat_xyzw[1] = m_GloveSplitFrameMgr.quat[skeletonIndex][2];
				ssd[skeletonIndex].quat_xyzw[2] = m_GloveSplitFrameMgr.quat[skeletonIndex][3];
				ssd[skeletonIndex].quat_xyzw[3] = m_GloveSplitFrameMgr.quat[skeletonIndex][0];

			}
			
			ret = 0;
		}

	}
	return ret;
}
