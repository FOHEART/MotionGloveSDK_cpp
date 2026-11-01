/**
 * @file  decodeAsGloveCSV.cpp
 * @brief 手套 CSV 字符流解帧的实现：帧头解析 + 多子包拼帧 + 骨骼字段落位。
 *
 * @details
 * 本文件实现 inc/decodeAsGloveCSV.h 声明的 decodeAsGloveCSV_Func()，
 * 内部使用进程级静态帧管理器 GloveSplitFrameMgr 累积同一帧号的多个子包。
 *
 * 实现约定（细节，参数与返回值语义以头文件声明处为准）：
 *  - 帧格式 "帧头,数据段"：帧头以空格分词，数据段以逗号分词；
 *  - euler 姿态每段骨骼 6 个字段（pos xyz + euler xyz），
 *    quat 姿态每段 7 个字段（pos xyz + quat wxyz，线上顺序为 wxyz）；
 *  - 静态帧管理器按帧号分帧，帧号变化即开始新帧；
 *  - 非有限角度在下游 EulerToQuat() 内被拒绝，不会写成 NaN。
 */
#include "inc/decodeAsGloveCSV.h"
#include <vector>
#include <algorithm>
#include "inc/eulerToQuat.h"

/**
 * @brief 手套 CSV 流"帧累积器"：按帧号缓存子包数据，收齐后解析为骨骼姿态。
 * @details 生命周期：进程级静态对象（本文件底部 m_GloveSplitFrameMgr），
 *          跨 decodeAsGloveCSV_Func() 调用保持状态；帧号变化时 reset()。
 *          非线程安全：同一时刻只应被一条数据流调用，多流并发未加锁。
 */
class GloveSplitFrameMgr
{
public:
	/** @brief 构造函数：调用 reset() 初始化到默认协议状态。 */
	GloveSplitFrameMgr(){ reset(); }
	/** @brief 析构函数：无动态资源，无需特殊处理。 */
	~GloveSplitFrameMgr(){}

	/**
	 * @brief 复位到初始状态：清空累积数据，恢复默认协议配置。
	 * @details 位置姿态默认 SkeletonPosition_None / SkeletonAttitude_Euler /
	 *          MVSDK_ZXY；全部 42 段骨骼的位置/欧拉角/四元数清零。
	 */
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

		markerFormat = GloveStreamFormat_Skeleton32;
		hasMarker = false;
		detectedSkeletonCount = 0;



		for (int i = 0; i < KHHS42_SkeletonCount; i++)
		{
			pos[i][0] = 0;
			pos[i][1] = 0; 
			pos[i][2] = 0;
		}
		for (int i = 0; i < KHHS42_SkeletonCount; i++)
		{
			euler[i][0] = 0;
			euler[i][1] = 0;
			euler[i][2] = 0;
		}
		for (int i = 0; i < KHHS42_SkeletonCount; i++)
		{
			quat[i][0] = 0;
			quat[i][1] = 0;
			quat[i][2] = 0;
			quat[i][3] = 0;
		}
	}

	/**
	 * @brief 用分隔符切分字符串，跳过空段。
	 * @param[in] str     待切分字符串，可为空串，结果为空向量。
	 * @param[in] pattern 分隔符，非空；相邻分隔符产生的空段会被丢弃。
	 * @return 非空子串的有序列表；输入为空时返回空向量。
	 */
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

	/**
	 * @brief 判断当前帧的子包是否已收齐。
	 * @return true 表示已收集的子包数 == 子包总数，可以解析；false 表示仍缺包。
	 */
	bool checkFrameComplete()
	{
		if (subFrameVec.size() == subFrameMax)
		{
			//printf("%s\r\n", m_data.c_str());
			return true;
		}

		return false;
	}

	/**
	 * @brief 把累积的整帧数据段解析到 pos/euler/quat 数组，euler 姿态换算四元数。
	 * @details 字段数必须等于 组字段数 x 42（euler 姿态 6 x 42 = 252，
	 *          quat 姿态 7 x 42 = 294），否则丢弃并返回 false。
	 *          euler 姿态下四元数由 EulerToQuat() 按 channelOrder 换算，
	 *          内部存储布局为 w,x,y,z。
	 * @return true 解析成功；false 字段个数不符（丢弃本帧）。
	 */
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

		// 先确定本帧段数：有标记时由标记决定，无标记时按列数消歧
		const size_t fieldCount = dataVec.size();
		unsigned int detected = 0;
		if (hasMarker)
		{
			if (GloveStreamFormat_Skeleton42 == markerFormat
				&& fieldCount == groupItemCount * KHHS42_SkeletonCount)
			{
				detected = KHHS42_SkeletonCount;
			}
			else if (GloveStreamFormat_Skeleton32 == markerFormat
				&& fieldCount == groupItemCount * KHHS32_SkeletonCount)
			{
				detected = KHHS32_SkeletonCount;
			}
		}
		else
		{
			// 无标记：现网 42 段帧同样不带标记，靠列数区分；
			// 同一 pos/姿态配置下 32 与 42 的列数互不相等，故无歧义
			if (fieldCount == groupItemCount * KHHS42_SkeletonCount)
				detected = KHHS42_SkeletonCount;
			else if (fieldCount == groupItemCount * KHHS32_SkeletonCount)
				detected = KHHS32_SkeletonCount;
		}
		if (0 == detected)
		{
			printf("[err]");
			return false;
		}
		detectedSkeletonCount = detected;

		for (unsigned int skeletonIndex = 0; skeletonIndex < detected; skeletonIndex++)
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
			for (unsigned int skeletonIndex = 0; skeletonIndex < detected; skeletonIndex++)
			{
				float tempQuat_xyzw[4] = { 0, 0, 0, 1 };
				EulerToQuat(
					euler[skeletonIndex][0],
					euler[skeletonIndex][1],
					euler[skeletonIndex][2],
					channelOrder,
					tempQuat_xyzw
					);
				// 内部布局：第 0 位 w，随后 x,y,z（EulerToQuat 输出为 x,y,z,w）
				quat[skeletonIndex][0] = tempQuat_xyzw[3];
				quat[skeletonIndex][1] = tempQuat_xyzw[0];
				quat[skeletonIndex][2] = tempQuat_xyzw[1];
				quat[skeletonIndex][3] = tempQuat_xyzw[2];
			}

		}

		return true;
	}

	/// 当前帧骨骼位置是否包含（SkeletonPosition_Meter 时才解析 pos）。
	SkeletonPosition_TypeDef skeletonPosition;
	/// 当前帧骨骼姿态格式（Euler 或 Quat）。
	SkeletonAttitude_TypeDef skeletonAttitude;
	/// 欧拉角的旋转顺序（euler 姿态时生效）。
	MVSDK_ChannelOrder channelOrder;

	/// 数据流名称（暂未使用）。
	std::string m_name;
	/// 累积的数据段；m_data 拼完整（子包收齐）后再解析。
	std::string m_data;

	/// 帧头声明的骨骼段数格式（仅 hasMarker 为 true 时有效）。
	GloveStreamSkeletonFormat_TypeDef markerFormat;
	/// 本帧帧头是否带骨骼段数标记（skeleton_32 / skeleton_42 / skeleton_openxr）。
	bool hasMarker;
	/// 最近一次解析出的骨骼段数（32 或 42），由 parseFrame() 写入。
	unsigned int detectedSkeletonCount;

	/// 当前帧号，帧号变化即开始新帧。
	unsigned int fn;
	/// 已收集的子包序号，去重与完整度计数用。
	std::vector<unsigned int> subFrameVec;
	/// 子包总数（subpackage a/b 中的 b）。
	unsigned int subFrameMax;
	/// 42 段骨骼的位置，单位米，索引即骨骼编号。
	float pos[KHHS42_SkeletonCount][3];
	/// 42 段骨骼的欧拉角，单位度。
	float euler[KHHS42_SkeletonCount][3];
	/// 42 段骨骼的四元数，内部布局为 w,x,y,z。
	float quat[KHHS42_SkeletonCount][4];
};
/** @brief 文件级分帧管理器：跨调用缓存各数据流的子包拼接状态（按帧号识别新帧）。 */
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
	if (KHHS32_SkeletonCount != skeletonCount && KHHS42_SkeletonCount != skeletonCount)
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
	sh->leftFigureSkeletonCount = 0;
	sh->rightFigureSkeletonCount = 0;

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

	bool hasMarker = false;
	GloveStreamSkeletonFormat_TypeDef markerFormat = GloveStreamFormat_Skeleton32;

	if (std::count(splitHeaderVec.begin(), splitHeaderVec.end(), "fn"))
	{
		auto it = std::find(splitHeaderVec.begin(), splitHeaderVec.end(), "fn");
		int dis = std::distance(splitHeaderVec.begin(), it);

		int fnDis = dis + 1;

		int fnConverted = std::stoi(splitHeaderVec[fnDis], 0, 10);
		sh->frameNumber = fnConverted;

		/* 帧号之后若还有 token，则可能是骨骼段数标记（标记固定紧跟 fn） */
		if ((fnDis + 1) < (int)splitHeaderVec.size())
		{
			const std::string& marker = splitHeaderVec[fnDis + 1];

			if ("skeleton_32" == marker)
			{
				hasMarker = true;
				markerFormat = GloveStreamFormat_Skeleton32;
			}
			else if ("skeleton_42" == marker)
			{
				hasMarker = true;
				markerFormat = GloveStreamFormat_Skeleton42;
			}
			else if ("skeleton_openxr" == marker)
			{
				/* 布局尚未定义：整帧丢弃，且不得触碰分帧管理器状态 */
				sh->skeletonFormat = GloveStreamFormat_OpenXR;
				return MGSDK_DECODE_OPENXR_UNSUPPORTED;
			}
			else if (0 == marker.compare(0, 9, "skeleton_"))
			{
				/* 未知标记：整帧丢弃，绝不回退成 32 段继续解析 */
				return -1;
			}
		}
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

			m_GloveSplitFrameMgr.hasMarker = hasMarker;
			m_GloveSplitFrameMgr.markerFormat = markerFormat;


			m_GloveSplitFrameMgr.fn = sh->frameNumber;
			m_GloveSplitFrameMgr.subFrameVec.push_back(subPak1);
			m_GloveSplitFrameMgr.subFrameMax = subPak2;

			std::string s = binStreamStr.substr(header.size() + 1);
			m_GloveSplitFrameMgr.m_data += s;
		}
		else
		{
			//add to current frame
			if (m_GloveSplitFrameMgr.hasMarker != hasMarker
				|| m_GloveSplitFrameMgr.markerFormat != markerFormat)
			{
				/* 同一帧的各子包都带同一个帧头，标记不一致说明串帧或发送端异常 */
				m_GloveSplitFrameMgr.reset();
				return -1;
			}
			m_GloveSplitFrameMgr.subFrameVec.push_back(subPak1);
			std::string s = binStreamStr.substr(header.size() + 1);
			m_GloveSplitFrameMgr.m_data += s;
		}
		bool isComplete = m_GloveSplitFrameMgr.checkFrameComplete();

		if (isComplete&&m_GloveSplitFrameMgr.parseFrame())
		{
			const unsigned int detected = m_GloveSplitFrameMgr.detectedSkeletonCount;
			if (detected > skeletonCount)
			{
				/* 调用方给出的输出容量不足以承载本帧 */
				return -1;
			}

			sh->skeletonFormat = (KHHS42_SkeletonCount == detected)
				? GloveStreamFormat_Skeleton42 : GloveStreamFormat_Skeleton32;
			sh->leftFigureSkeletonCount = (KHHS42_SkeletonCount == detected) ? 20 : 15;
			sh->rightFigureSkeletonCount = sh->leftFigureSkeletonCount;

			for (unsigned int skeletonIndex = 0; skeletonIndex < detected; skeletonIndex++)
			{
				if (KHHS42_SkeletonCount == detected)
				{
					ssd[skeletonIndex].gloveSkeletonIndex = (kinemHumanHandsSkeleton42Index)(skeletonIndex);

					kinemHumanHandsSkeleton42[skeletonIndex].copy(
						ssd[skeletonIndex].skeletonName,
						kinemHumanHandsSkeleton42[skeletonIndex].length(),
						0);
				}
				else
				{
					ssd[skeletonIndex].gloveSkeletonIndex32 = (kinemHumanHandsSkeleton32Index)(skeletonIndex);

					kinemHumanHandsSkeleton32[skeletonIndex].copy(
						ssd[skeletonIndex].skeletonName,
						kinemHumanHandsSkeleton32[skeletonIndex].length(),
						0);
				}

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
