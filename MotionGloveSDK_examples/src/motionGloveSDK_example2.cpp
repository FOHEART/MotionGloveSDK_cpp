/**
 * @file  motionGloveSDK_example2.cpp
 * @brief 示例 2：绑定 UDP 端口接收数据流，按帧输出骨骼姿态数据。
 * @ingroup mgsdk_examples
 *
 * @details
 * 在示例 1 的收帧闭环上，进一步打印取回帧内的骨骼信息
 * （帧号 / 骨骼数量 / 姿态数据），并额外打印左手拇指第 3 段与拇指末端
 * 的位置（米）与欧拉角（度）。32 段布局没有末端骨骼，此时该列显示 N/A。
 */
#ifdef _WIN32
#include <windows.h>

#else
#include <unistd.h>
#endif
#include <stdio.h>

#include <string>
#include "motionGloveSDK.h"
#include "motionGloveSDK_exampleCommon.h"

/** @brief 使用说明文本：程序启动后打印到控制台，说明运行流程。 */
static const char* HEADER_COMMENT =
    u8"/*******motionGloveSDK_example2*********/\n"
    u8"本程序用来测试左右手手掌的角度，以及左手拇指第3段与拇指末端的位置和角度\n"
    u8"位置单位米，角度单位度；拇指末端仅在42段骨骼格式中存在\n"
    u8"/*******************************************/\n";

/**
 * @brief 程序入口：绑定本机 UDP 端口并循环读取手套数据。
 *        端口号要与 MotionGlove 中 设置->选项->插件->数据转发 中的端口号一致。
 * @param argc [in] 命令行参数个数。
 * @param argv [in] 命令行参数数组。
 * @return 0 表示正常退出；-1 表示 UDP 绑定失败。
 */
int main(int argc, char* argv[])
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
    printf("%s", HEADER_COMMENT);

    int rxPortDefault = 5001;

    printf("UDP Bind IP:port: 127.0.0.1:%d\n", rxPortDefault);
    int nRet = MotionGloveSDK_ListenUDPPort(rxPortDefault);

    if (nRet == -1)
    {
        printf("Error code %d, any key to exit.\n", nRet);
        getchar();
        return 0;
    }
    else
    {
        printf("[UDP]Bind port %d success. Start Reading Data...\n", rxPortDefault);
    }

    /*
    *注册一个回调函数 每当sdk收到一帧有效的数据 就回调一次函数
    *所有与手部姿态位置等相关的信息 都可以从函数内部获取到
    *
    获取手部骨骼姿态：示例会先查询当前帧的骨骼格式，自动适配 32 段与 42 段两种数据流。
    *
    */
    /* 兼容两种数据流格式：32 段与 42 段各有独立的容器与取帧接口 */
    KHHS42PosAttitude_TypeDef KHHS42PosAttitude;
    KHHS32PosAttitude_TypeDef KHHS32PosAttitude;
    while (true)
    {
        /*
        接收到的数据包为60Hz
        */
        std::string gloveName = "Glove1";
        static int frameCounter = 0;
        if (MotionGloveSDK_isGloveNewFramePending(gloveName.c_str()))
        {
            //printf("[%d]New frame received\n", frameCounter++);

            GloveStreamSkeletonFormat_TypeDef frameType = GloveStreamFormat_Skeleton42;
            (void)MotionGloveSDK_GetSkeletonsFrameType(gloveName.c_str(), &frameType);
            const bool isLayout42 = (GloveStreamFormat_Skeleton42 == frameType);
            const int getRet = isLayout42
                ? MotionGloveSDK_GetSkeletons42Frame(gloveName.c_str(), &KHHS42PosAttitude)
                : MotionGloveSDK_GetSkeletons32Frame(gloveName.c_str(), &KHHS32PosAttitude);
            const SingleSkeletonDef* bones = isLayout42
                ? KHHS42PosAttitude.skeletons : KHHS32PosAttitude.skeletons;

            if (0 == getRet)
            {
                const StreamHeader& hdr = isLayout42 ? KHHS42PosAttitude.hdr : KHHS32PosAttitude.hdr;
#if 1
                SingleSkeletonDef leftPalm = bones[isLayout42 ? KHHS42_LeftHand : KHHS32_LeftHand];
                SingleSkeletonDef rightPalm = bones[isLayout42 ? KHHS42_RightHand : KHHS32_RightHand];
                PrintGloveFrameHeader(hdr);
                printf("Left Palm Euler Angle: [%.2f, %.2f, %.2f] ",
                    leftPalm.euler_degree[0],
                    leftPalm.euler_degree[1],
                    leftPalm.euler_degree[2]);
                printf(" Right Palm Euler Angle: [%.2f, %.2f, %.2f] ",
                    rightPalm.euler_degree[0],
                    rightPalm.euler_degree[1],
                    rightPalm.euler_degree[2]);
                printf("\r\n");
#endif
#if 0
                /* 左手拇指第 3 段：位置（米）+ 欧拉角（度）；末端骨骼仅在 42 段布局中存在 */
                const SingleSkeletonDef& leftThumb3 = bones[isLayout42 ? KHHS42_LeftHandThumb3 : KHHS32_LeftHandThumb3];
                PrintGloveFrameHeader(hdr);
                printf("Left Thumb3 Pos(m): [%.4f, %.4f, %.4f] Euler(deg): [%.2f, %.2f, %.2f] ",
                    leftThumb3.position_meter[0],
                    leftThumb3.position_meter[1],
                    leftThumb3.position_meter[2],
                    leftThumb3.euler_degree[0],
                    leftThumb3.euler_degree[1],
                    leftThumb3.euler_degree[2]);
                if (isLayout42)
                {
                    const SingleSkeletonDef& leftThumb3End = bones[KHHS42_LeftHandThumb3End];
                    printf("Left Thumb3End Pos(m): [%.4f, %.4f, %.4f] Euler(deg): [%.2f, %.2f, %.2f]",
                        leftThumb3End.position_meter[0],
                        leftThumb3End.position_meter[1],
                        leftThumb3End.position_meter[2],
                        leftThumb3End.euler_degree[0],
                        leftThumb3End.euler_degree[1],
                        leftThumb3End.euler_degree[2]);
                }
                else
                {
                    printf("Left Thumb3End: N/A (32-segment layout has no fingertip)");
                }
                printf("\r\n");
                #endif
            }
            else
            {
                printf("Get glove skeleton frame failed!\n");

            }


            MotionGloveSDK_resetGloveNewFramePending(gloveName.c_str());
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000); // 10ms delay, equivalent to Windows Sleep(10)
#endif

    }

    return 0;
}