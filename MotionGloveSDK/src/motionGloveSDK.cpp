/**
 * @file  motionGloveSDK.cpp
 * @brief SDK 对外 API 实现：UDP 接收线程、数据流缓存与骨骼帧输出。
 * @ingroup mgsdk_api
 *
 * @details
 * 本文件实现 include/motionGloveSDK.h 声明的对外接口。
 * 线程模型：接收线程在 MotionGloveSDK_ListenUDPPort() 中启动，按数据流
 * 名称缓存最新帧；查询/取帧接口由调用方线程触发，内部用读写锁保护缓存。
 * 平台差异：Windows 使用 Winsock2，Linux 使用 POSIX socket。
 */
#ifdef _WIN32

#include <windows.h>
#include <Winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <set>
#include <sstream>
#include <errno.h>
#include "include/motionGloveSDK.h"
#include "inc/motionGloveSdkHelper.h"
#include "inc/decodeAsGloveCSV.h"
#include "inc/readWriteLock.h"

/** @brief 手套数据流管理器数组的最大条数（最多同时管理的数据流数）。 */
#define SUIT_MGR_MAXLEN (32)

/** @brief 保护 dataset 的读写锁：所有查询/新增/取帧操作需持锁访问。 */
ReadWriteLock gloveSuitManagerLock;

/** @brief 手套数据流管理器数组：按数据流名称缓存最新帧状态，条目数不超过 SUIT_MGR_MAXLEN。 */
GloveSuitManager_TypeDef gloveSuitManager[SUIT_MGR_MAXLEN];

/** @brief SDK 全局运行状态，随 UDP 打开/线程启停而更新。 */
SDK_Status_TypeDef sdk_status = SDK_Status_None;

/**
 * @brief Socket used for UDP communication.
 * @brief (中文) UDP 通信使用的套接字
 */
#ifdef _WIN32
SOCKET socketObj = INVALID_SOCKET;
#else
int socketObj = -1; // Linux下socket是int类型
#endif

/**
 * @brief Error message buffer.
 * @brief (中文) 错误信息缓冲区
 */
char  errorMsg[100] = { ' ' };

#ifdef _WIN32
/**
 * @brief Windows socket initialization data.
 * @brief (中文) Windows 套接字初始化数据
 */
WSADATA wsaData;
#endif

/** @brief 单数据流的"新帧待取"标记：接收线程置位，取帧后复位。 */
bool isNewPosePending = false;
/** @brief 已接收但尚未取走的新帧计数。 */
int newPoseCount = 0;

/**
 * @brief Global mutex protecting shared state.
 * @brief (中文) 保护共享状态的全局互斥锁
 */
#ifdef _WIN32
CRITICAL_SECTION m_cs;
#else
pthread_mutex_t m_cs = PTHREAD_MUTEX_INITIALIZER;
#endif

/** @brief 最近一帧完整骨骼姿态数据的全局暂存区（收包线程写入）。 */
KHHS42PosAttitude_TypeDef KHHS42PosAttitude;

/** @brief 清空全部数据流管理器条目（逐条持写锁 memset），仅在接收线程启动时调用。 */
void resetGloveSuitManager()
{
	int arrLen = sizeof(gloveSuitManager) / sizeof(gloveSuitManager[0]);
	for (int i = 0; i < arrLen; i++)
	{
		gloveSuitManagerLock.writeLock();
		memset((void*)(&(gloveSuitManager[i])), 0x00, sizeof(GloveSuitManager_TypeDef));
		gloveSuitManagerLock.writeUnLock();
	}
}

/**
 * @brief 清空一帧骨骼姿态数据（整帧 memset 清零）。
 * @param input [in,out] 待清零的帧数据指针，可为空（由调用方保证有效性）。
 */
void resetKHHS42PosAttitude(KHHS42PosAttitude_TypeDef* input)
{
	memset((void*)input, 0x00, sizeof(KHHS42PosAttitude_TypeDef));
}

/**
 * @brief 清空一帧 32 段骨骼姿态数据（整帧 memset 清零）。
 * @param input [in,out] 待清零的帧数据指针，可为空（由调用方保证有效性）。
 */
void resetKHHS32PosAttitude(KHHS32PosAttitude_TypeDef* input)
{
	memset((void*)input, 0x00, sizeof(KHHS32PosAttitude_TypeDef));
}

/**
 * @brief 接收线程入口：清空管理器后进入 recvfrom 循环，
 *        按帧头解析数据流、更新"新帧待取"标记与最新帧暂存区。
 * @param lpParam [in] 线程参数，当前未使用，固定传 NULL。
 * @return Windows 分支返回线程退出码 0；Linux 分支返回 NULL。
 */
//thread function
#ifdef _WIN32
static DWORD WINAPI ThreadProc(LPVOID lpParam)
#else
static void* ThreadProc(void* lpParam)
#endif
{
	resetGloveSuitManager();

	//socket
	struct sockaddr_in from;
#ifdef _WIN32
	int fromlen = sizeof(from);
#else
	socklen_t fromlen = sizeof(from);
#endif

	const static int maxFrameLen = 32 * 1024;
	//recv buffer 如果开启巨型帧 最大可达到9KB

	char* rxBuffer = (char*)calloc(1, maxFrameLen);
	if (rxBuffer == 0)
	{
#ifdef _WIN32
		return -1;
#else
		return NULL;
#endif
	}

	while (1)
	{
		if (SDK_Status_ThreadPause == sdk_status)
		{
#ifdef _WIN32
			Sleep(100);
#else
			usleep(100 * 1000); // 100ms
#endif
			continue;
		}
#ifdef _WIN32
		if (socketObj == INVALID_SOCKET)
#else
		if (socketObj == -1)
#endif
		{
			break;
		}

		/* 等待数据可读（10ms 超时）后再收包：
		   原实现每包 Sleep(1)，Windows 下实际休眠约 15.6ms，
		   60Hz 大帧（42 段 CSV，通常 2~3KB/包）时接收吞吐不足、
		   接收缓冲区溢出丢包，表现为"骨骼不动/偶发跳帧"。
		   现改为 select 等待 + 非阻塞 recvfrom，并把一次 select
		   唤醒后可读的包全部处理完，再回到 select 等待。 */
		{
#ifdef _WIN32
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(socketObj, &readSet);
			timeval timeout = { 0, 10 * 1000 };
			int selRet = select((int)socketObj + 1, &readSet, NULL, NULL, &timeout);
			if (selRet <= 0)
			{
				continue;
			}
#else
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(socketObj, &readSet);
			timeval timeout = { 0, 10 * 1000 };
			int selRet = select(socketObj + 1, &readSet, NULL, NULL, &timeout);
			if (selRet <= 0)
			{
				continue;
			}
#endif
		}

		/* 一次性收空当前接收队列（非阻塞模式，队列空时 recvfrom 返回 EWOULDBLOCK）。 */
		while (1)
		{
			int nRet = recvfrom(socketObj, rxBuffer, maxFrameLen, 0, (struct sockaddr*)&from, &fromlen);
#ifdef _WIN32
			if (nRet == SOCKET_ERROR)
			{
				break;
			}
#else
			if (nRet < 0)
			{
				break;
			}
#endif
			rxBuffer[nRet] = 0;
#ifdef _WIN32
			KHHS42PosAttitude.remoteIP[0] = from.sin_addr.S_un.S_un_b.s_b1;
			KHHS42PosAttitude.remoteIP[1] = from.sin_addr.S_un.S_un_b.s_b2;
			KHHS42PosAttitude.remoteIP[2] = from.sin_addr.S_un.S_un_b.s_b3;
			KHHS42PosAttitude.remoteIP[3] = from.sin_addr.S_un.S_un_b.s_b4;
#else
			// Linux下IP地址处理
			uint32_t ip = ntohl(from.sin_addr.s_addr);
			KHHS42PosAttitude.remoteIP[0] = (ip >> 24) & 0xFF;
			KHHS42PosAttitude.remoteIP[1] = (ip >> 16) & 0xFF;
			KHHS42PosAttitude.remoteIP[2] = (ip >> 8) & 0xFF;
			KHHS42PosAttitude.remoteIP[3] = ip & 0xFF;
#endif

			KHHS42PosAttitude.remotePort = from.sin_port;
			KHHS42PosAttitude.sdkVersion = MotionGloveSDK_getVersion();
			//printf("FrameLength:%d Bytes\r\n", nRet);
			if (0 == isFrameCSVFormat(rxBuffer))
			{
				/*
				不是json字符串 有可能是csv字符串
				*/
				if (0 == isFrameCSV_Glove(rxBuffer))
				{
					KHHS42PosAttitude_TypeDef kpa;
					resetKHHS42PosAttitude(&kpa);

#ifdef _WIN32
					kpa.remoteIP[0] = from.sin_addr.S_un.S_un_b.s_b1;
					kpa.remoteIP[1] = from.sin_addr.S_un.S_un_b.s_b2;
					kpa.remoteIP[2] = from.sin_addr.S_un.S_un_b.s_b3;
					kpa.remoteIP[3] = from.sin_addr.S_un.S_un_b.s_b4;
#else
					kpa.remoteIP[0] = (ip >> 24) & 0xFF;
					kpa.remoteIP[1] = (ip >> 16) & 0xFF;
					kpa.remoteIP[2] = (ip >> 8) & 0xFF;
					kpa.remoteIP[3] = ip & 0xFF;
#endif

					kpa.remotePort = from.sin_port;
					kpa.sdkVersion = MotionGloveSDK_getVersion();

					int decodeRet = decodeAsGloveCSV_Func(rxBuffer,
						&kpa.hdr, 
						kpa.skeletons,
						KHHS42_SkeletonCount);
					if (MGSDK_DECODE_OPENXR_UNSUPPORTED == decodeRet)
					{
						/* 收到 OpenXR 布局：格式已识别但布局未定义，整帧丢弃。
						   按累计次数节流打印，避免异常发送端刷屏。 */
						static unsigned int openXrDropCount = 0;
						++openXrDropCount;
						if (1 == openXrDropCount || 0 == (openXrDropCount % 100))
						{
							printf("[warn] skeleton_openxr frame dropped (total %u)\r\n", openXrDropCount);
						}
						continue;
					}
					if (decodeRet == -1)
						continue;
					if (kpa.hdr.leftFigureSkeletonCount == 0)
					{
						//printf("");
					}
					int index = GloveSuitManager_SearchByName(gloveSuitManager, SUIT_MGR_MAXLEN, kpa.hdr.AvatarName, kpa.hdr.AvatarNameLength);
					if (index == -1)
					{
						/*没有找到，添加*/
						index = GloveSuitManager_getFirstUnOccupy(gloveSuitManager, SUIT_MGR_MAXLEN);
						index = GloveSuitManager_AddName(gloveSuitManager, SUIT_MGR_MAXLEN, kpa.hdr.AvatarName, kpa.hdr.AvatarNameLength);
					}
					if (index != -1)
					{
						GloveSuitManager_TypeDef* slot = &gloveSuitManager[index];
						gloveSuitManagerLock.writeLock();
						/*找到了名称相同的容器：按本帧格式写入对应容器*/
						if (GloveStreamFormat_Skeleton32 == kpa.hdr.skeletonFormat)
						{
							resetKHHS32PosAttitude(&slot->posAttitude32);
							memcpy(&slot->posAttitude32.hdr, &kpa.hdr, sizeof(StreamHeader));
							memcpy(slot->posAttitude32.skeletons, kpa.skeletons,
								sizeof(SingleSkeletonDef) * KHHS32_SkeletonCount);
							slot->posAttitude32.remoteIP[0] = kpa.remoteIP[0];
							slot->posAttitude32.remoteIP[1] = kpa.remoteIP[1];
							slot->posAttitude32.remoteIP[2] = kpa.remoteIP[2];
							slot->posAttitude32.remoteIP[3] = kpa.remoteIP[3];
							slot->posAttitude32.remotePort = kpa.remotePort;
							slot->posAttitude32.sdkVersion = kpa.sdkVersion;
						}
						else
						{
							resetKHHS42PosAttitude(&slot->posAttitude42);
							memcpy(&slot->posAttitude42, &kpa, sizeof(KHHS42PosAttitude_TypeDef));
						}
						slot->frameFormat = kpa.hdr.skeletonFormat;
						//printf("fn:%d\r\n", kpa.hdr.frameNumber);
						slot->isNewFramePending = 1;
						gloveSuitManagerLock.writeUnLock();
					}

					//printf("");
				}
			}
			else
			{
				/*
				不是json字符串 有可能是二进制字符
				*/
				uint16_t protocolVersion = 0;
				memcpy(&protocolVersion, rxBuffer, sizeof(protocolVersion));

				switch (protocolVersion)
				{
				// 协议版本处理
				}

				isNewPosePending = true;
				newPoseCount++;
			}
		}
	}

	if (rxBuffer)
	{
		free(rxBuffer);
		rxBuffer = 0;
	}
#ifdef _WIN32
	if (socketObj != INVALID_SOCKET)
	{
		closesocket((SOCKET)socketObj);
		socketObj = INVALID_SOCKET;
	}
	return 0;
#else
	if (socketObj != -1)
	{
		close(socketObj);
		socketObj = -1;
	}
	return NULL;
#endif
}

/** @brief 接收线程句柄（Windows 为 HANDLE，Linux 为 pthread_t），由这个文件唯一创建与销毁。 */
#ifdef _WIN32
HANDLE hThreadProc = nullptr;
#else
pthread_t hThreadProc = 0;
#endif

/**
 * @brief 打印占用指定 UDP 端口的进程信息（仅 Windows：netstat 查 PID 并枚举进程名）。
 * @param nPort 被占用的端口号。
 */
static void MotionGloveSDK_PrintUDPPortOccupier(int nPort)
{
#ifdef _WIN32
	char command[256] = { 0 };
	sprintf_s(command, "netstat -ano -p udp | findstr :%d", nPort);

	FILE* pipe = _popen(command, "r");
	if (!pipe)
	{
		printf("Failed to inspect port %d owner.\r\n", nPort);
		return;
	}

	char line[512] = { 0 };
	std::set<unsigned long> pids;
	while (fgets(line, sizeof(line), pipe) != NULL)
	{
		std::stringstream ss(line);
		std::string token;
		std::string lastToken;
		while (ss >> token)
		{
			lastToken = token;
		}
		if (!lastToken.empty())
		{
			char* endPtr = NULL;
			unsigned long pid = strtoul(lastToken.c_str(), &endPtr, 10);
			if (endPtr != NULL && *endPtr == '\0')
			{
				pids.insert(pid);
			}
		}
	}
	_pclose(pipe);

	if (pids.empty())
	{
		printf("Port %d is occupied, but process info was not found by netstat.\r\n", nPort);
		return;
	}

	for (std::set<unsigned long>::const_iterator it = pids.begin(); it != pids.end(); ++it)
	{
		sprintf_s(command, "tasklist /FI \"PID eq %lu\" /FO CSV /NH", *it);
		FILE* taskPipe = _popen(command, "r");
		if (!taskPipe)
		{
			printf("Port %d occupied by PID %lu\r\n", nPort, *it);
			continue;
		}

		char taskLine[512] = { 0 };
		if (fgets(taskLine, sizeof(taskLine), taskPipe) != NULL)
		{
			printf("Port %d occupied: %s", nPort, taskLine);
		}
		else
		{
			printf("Port %d occupied by PID %lu\r\n", nPort, *it);
		}
		_pclose(taskPipe);
	}
#else
	char command[256] = { 0 };
	snprintf(command, sizeof(command), "lsof -nP -iUDP:%d 2>/dev/null", nPort);

	FILE* pipe = popen(command, "r");
	if (pipe)
	{
		char line[512] = { 0 };
		bool hasOutput = false;
		while (fgets(line, sizeof(line), pipe) != NULL)
		{
			hasOutput = true;
			printf("%s", line);
		}
		pclose(pipe);
		if (hasOutput)
		{
			return;
		}
	}

	snprintf(command, sizeof(command), "ss -lunp '( sport = :%d )' 2>/dev/null", nPort);
	pipe = popen(command, "r");
	if (!pipe)
	{
		printf("Failed to inspect port %d owner.\n", nPort);
		return;
	}

	char line[512] = { 0 };
	bool hasOutput = false;
	while (fgets(line, sizeof(line), pipe) != NULL)
	{
		hasOutput = true;
		printf("%s", line);
	}
	pclose(pipe);

	if (!hasOutput)
	{
		printf("Port %d is occupied, but process info was not found by lsof/ss.\n", nPort);
	}
#endif
}

/**
 * @brief 关闭旧套接字并重新创建、绑定 UDP 套接字到本地端口。
 * @param local 待绑定的本地地址结构（INADDR_ANY 表示任意网卡）。
 * @param nPort 绑定的本地端口号。
 * @return 0 绑定成功；-1 创建/绑定失败。
 */
static int MotionGloveSDK_ReopenAndBindSocket(const struct sockaddr_in* local, int nPort)
{
#ifdef _WIN32
	if (socketObj != INVALID_SOCKET)
	{
		closesocket((SOCKET)socketObj);
		socketObj = INVALID_SOCKET;
	}
#else
	if (socketObj != -1)
	{
		/* close() 唤醒不了阻塞在 recvfrom 的接收线程，先 shutdown()（同 CloseUDPPort） */
		shutdown(socketObj, SHUT_RDWR);
		close(socketObj);
		socketObj = -1;
	}
#endif

	socketObj = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
	if (socketObj == INVALID_SOCKET)
#else
	if (socketObj == -1)
#endif
	{
		return -1;
	}

	/* 接收线程用"select 等待 + 连续 recvfrom 清空队列"的模式，
	   套接字必须是非阻塞的，否则队列收空后 recvfrom 会卡死。 */
#ifdef _WIN32
	{
		u_long nonBlocking = 1;
		ioctlsocket(socketObj, FIONBIO, &nonBlocking);
	}
#else
	{
		int flags = fcntl(socketObj, F_GETFL, 0);
		if (flags != -1)
		{
			fcntl(socketObj, F_SETFL, flags | O_NONBLOCK);
		}
	}
#endif

	int nRecvBuf = 1024 * 1024;//接收缓冲区 1MB：60Hz 的大帧（42 段 CSV 可达 3KB/包）下避免溢出丢包
	setsockopt(socketObj, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));

	int bindRet = bind(socketObj, (struct sockaddr*)local, sizeof(*local));
	if (bindRet != 0)
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		printf("Bind local port %d failed! WSA error: %d\r\n", nPort, err);
		if (err == WSAEADDRINUSE)
		{
			MotionGloveSDK_PrintUDPPortOccupier(nPort);
		}
		closesocket((SOCKET)socketObj);
		socketObj = INVALID_SOCKET;
#else
		int err = errno;
		printf("Bind local port %d failed! errno: %d\n", nPort, err);
		if (err == EADDRINUSE)
		{
			MotionGloveSDK_PrintUDPPortOccupier(nPort);
		}
		close(socketObj);
		socketObj = -1;
#endif
		return -1;
	}

	return 0;
}

 unsigned int MotionGloveSDK_getVersion(void)
{
	 unsigned int mainVersion = 0;
	 unsigned int subVersion = 0;
	 unsigned int patchVersion = 9;
	 return (mainVersion << 16) | (subVersion << 8) | patchVersion;
}

int MotionGloveSDK_ListenUDPPort(int nPort)
{
	int nRet = 0;
	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(nPort); ///监听端口
	local.sin_addr.s_addr = INADDR_ANY; ///本机

#ifdef _WIN32
	{
		if (WSAStartup(MAKEWORD(2, 1), &wsaData)) //调用Windows Sockets DLL
		{
			memcpy(errorMsg, "socket init failed.\0", sizeof(errorMsg));
			WSACleanup();
			return FALSE;
		}
	}
#endif

	if (SDK_Status_None == sdk_status)
	{
		if (MotionGloveSDK_ReopenAndBindSocket(&local, nPort) == 0)
		{
			sdk_status = SDK_Status_OpenUDP;
		}
		else
		{
			nRet = -1;
		}
	}

	if (SDK_Status_OpenUDP == sdk_status)
	{
		/*socket打开了 那么就尝试创建线程*/
#ifdef _WIN32
		hThreadProc = CreateThread(
			NULL,              // default security attributes
			0,                 // use default stack size  
			ThreadProc,        // thread function 
			(void*)0,             // argument to thread function 
			CREATE_SUSPENDED,                 // use default creation flags 
			NULL);           // returns the thread identifier 
		if (0 != hThreadProc)
		{
			ResumeThread(hThreadProc);
			sdk_status = SDK_Status_ThreadRunning;
		}
		else
		{
			nRet += -1;
		}
#else
		int threadRet = pthread_create(&hThreadProc, NULL, ThreadProc, NULL);
		if (threadRet == 0)
		{
			sdk_status = SDK_Status_ThreadRunning;
		}
		else
		{
			nRet += -1;
		}
#endif
	}

	if (SDK_Status_ThreadRunning == sdk_status)
	{
		// 线程运行中
	}

	if (SDK_Status_ThreadPause == sdk_status)
	{
		if (MotionGloveSDK_ReopenAndBindSocket(&local, nPort) == 0)
		{
			sdk_status = SDK_Status_ThreadRunning;
		}
		else
		{
			nRet += -1;
		}
	}

	return nRet;
}

int MotionGloveSDK_CloseUDPPort(void)
{
	int ret = -1;
	bool hasClosedSocket = false;

	if (sdk_status == SDK_Status_ThreadRunning || sdk_status == SDK_Status_OpenUDP || sdk_status == SDK_Status_ThreadPause)
	{
#ifdef _WIN32
		if (socketObj != INVALID_SOCKET)
		{
			closesocket((SOCKET)socketObj);
			socketObj = INVALID_SOCKET;
			hasClosedSocket = true;
		}

		if (hThreadProc != nullptr)
		{
			DWORD waitRet = WaitForSingleObject(hThreadProc, 1000);
			if (waitRet == WAIT_OBJECT_0)
			{
				CloseHandle(hThreadProc);
				hThreadProc = nullptr;
			}
			else
			{
				printf("CloseUDPPort wait thread timeout, ret=%lu\r\n", waitRet);
			}
		}

		if (hasClosedSocket)
		{
			WSACleanup();
		}
#else
		if (socketObj != -1)
		{
			/* Linux 下 close() 不会唤醒阻塞在 recvfrom 的接收线程，
			   先用 shutdown() 让它返回，否则下面的 pthread_join 会永久阻塞 */
			shutdown(socketObj, SHUT_RDWR);
			close(socketObj);
			socketObj = -1;
			hasClosedSocket = true;
		}

		if (hThreadProc != 0)
		{
			pthread_join(hThreadProc, NULL);
			hThreadProc = 0;
		}
#endif

		sdk_status = SDK_Status_None;
		ret = hasClosedSocket ? 0 : -1;
	}

	return ret;
}

bool MotionGloveSDK_isGloveNewFramePending(const char* actorName)
{
	bool  ret = false;
	int actualNameLen = 0;
	for (int i = 0; i < ACTOR_NAME_LEN_MAX; i++)
	{
		if (actorName[i] != 0)
		{
			actualNameLen++;
		}
		else
		{
			break;
		}
	}
	if (actualNameLen == 0)
	{
		return ret;
	}

	int index = GloveSuitManager_SearchByName(gloveSuitManager, SUIT_MGR_MAXLEN, (uint8_t*)actorName, actualNameLen);
	if (index != -1)
	{
		if (gloveSuitManager[index].isNewFramePending !=0)
		{
			ret = true;
		}
		else
		{
			ret = false;
		}
	}

	return ret;
}

bool MotionGloveSDK_resetGloveNewFramePending(const char* actorName)
{
	bool ret = false;

	int actualNameLen = 0;
	for (int i = 0; i < ACTOR_NAME_LEN_MAX; i++)
	{
		if (actorName[i] != 0)
		{
			actualNameLen++;
		}
		else
		{
			break;
		}
	}
	if (actualNameLen == 0)
		return ret;

	int index = GloveSuitManager_SearchByName(gloveSuitManager, SUIT_MGR_MAXLEN, (uint8_t*)actorName, actualNameLen);
	if (index != -1)
	{
		gloveSuitManagerLock.writeLock();
		gloveSuitManager[index].isNewFramePending = 0;
		gloveSuitManagerLock.writeUnLock();
		ret = true;
	}

	return ret;
}

/**
 * @brief 按数据流名称定位管理器槽位下标。
 * @param actorName [in] NUL 结尾的数据流名称；为空串时视为非法。
 * @return 命中的槽位下标 [0, SUIT_MGR_MAXLEN)；名称为空或未找到时返回 -1。
 */
static int findActorIndex(const char* actorName)
{
	if (nullptr == actorName)
		return -1;

	int actualNameLen = 0;
	for (int i = 0; i < ACTOR_NAME_LEN_MAX; i++)
	{
		if (actorName[i] != 0)
		{
			actualNameLen++;
		}
		else
		{
			break;
		}
	}
	if (actualNameLen == 0)
		return -1;

	return GloveSuitManager_SearchByName(gloveSuitManager, SUIT_MGR_MAXLEN,
		(uint8_t*)actorName, actualNameLen);
}

int  MotionGloveSDK_GetSkeletonsFrameType(const char* actorName, GloveStreamSkeletonFormat_TypeDef* pFormat)
{
	if (nullptr == pFormat)
		return -1;

	int index = findActorIndex(actorName);
	if (index == -1)
		return -1;

	gloveSuitManagerLock.readLock();
	*pFormat = gloveSuitManager[index].frameFormat;
	gloveSuitManagerLock.readUnLock();

	return 0;
}

int  MotionGloveSDK_GetSkeletons42Frame(const char* actorName, KHHS42PosAttitude_TypeDef* pKHHS42PosAttitude)
{
	if (nullptr == pKHHS42PosAttitude)
		return -1;

	int index = findActorIndex(actorName);
	if (index == -1)
		return -1;

	gloveSuitManagerLock.readLock();
	if (GloveStreamFormat_Skeleton42 != gloveSuitManager[index].frameFormat)
	{
		gloveSuitManagerLock.readUnLock();
		return -1;
	}
	memcpy((uint8_t*)pKHHS42PosAttitude,
		(uint8_t*)&gloveSuitManager[index].posAttitude42,
		sizeof(KHHS42PosAttitude_TypeDef));
	gloveSuitManagerLock.readUnLock();

	return 0;
}

int  MotionGloveSDK_GetSkeletons32Frame(const char* actorName, KHHS32PosAttitude_TypeDef* pKHHS32PosAttitude)
{
	if (nullptr == pKHHS32PosAttitude)
		return -1;

	int index = findActorIndex(actorName);
	if (index == -1)
		return -1;

	gloveSuitManagerLock.readLock();
	if (GloveStreamFormat_Skeleton32 != gloveSuitManager[index].frameFormat)
	{
		gloveSuitManagerLock.readUnLock();
		return -1;
	}
	memcpy((uint8_t*)pKHHS32PosAttitude,
		(uint8_t*)&gloveSuitManager[index].posAttitude32,
		sizeof(KHHS32PosAttitude_TypeDef));
	gloveSuitManagerLock.readUnLock();

	return 0;
}

int  MotionGloveSDK_GetSkeletonsOpenXRFrame(const char* actorName, KHHSOpenXRPosAttitude_TypeDef* pKHHSOpenXRPosAttitude)
{
	/* OpenXR 布局尚未定义，SDK 当前也收不到该格式：接口签名与容器先行冻结，实现留待布局确定。 */
	(void)actorName;
	(void)pKHHSOpenXRPosAttitude;
	return -1;
}