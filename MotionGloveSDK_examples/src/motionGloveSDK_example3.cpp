/**
 * @file  motionGloveSDK_example3.cpp
 * @brief 示例 3：绑定 UDP 端口接收数据流，按帧输出骨骼姿态数据（变体）。
 * @ingroup mgsdk_examples
 *
 * @details
 * 与示例 2 同模式的接收闭环，展示读取侧代码组织方式的另一种写法，
 * 便于对照修改自己的接入代码。
 */
#ifdef _WIN32
#include <windows.h>

#else
#include <unistd.h>
#endif
#include <stdio.h>

#include <string>
#include "motionGloveSDK.h"

/** @brief 使用说明文本：程序启动后打印到控制台，说明运行流程。 */
static const char* HEADER_COMMENT =
    u8"/*******motionGloveSDK_example3*********/\n"
    u8"本程序用来测试左手拇指三段骨骼的角度\n"
    u8"拇指第1段的角度为相对于手背的角度\n"
    u8"拇指第2段的角度为相对于拇指第1段的角度\n"
    u8"拇指第3段的角度为相对于拇指第2段的角度\n"
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
    HandsPosAttitudeCB_Regist函数是注册获取32段手指骨骼的回调函数。适用于FOHEART HMAX数据手套。
    *
    */
    KHHS42PosAttitude_TypeDef KHHS42PosAttitude;
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

            if (0 == MotionGloveSDK_GetSkeletons42Frame(gloveName.c_str(), &KHHS42PosAttitude))
            {
                SingleSkeletonDef leftThumb1 = KHHS42PosAttitude.skeletons[KHHS42_LeftHandThumb1];
                SingleSkeletonDef leftThumb2 = KHHS42PosAttitude.skeletons[KHHS42_LeftHandThumb2];
                SingleSkeletonDef leftThumb3 = KHHS42PosAttitude.skeletons[KHHS42_LeftHandThumb3];
                printf("Frame [%d] Left Thumb Angles: Thumb1[%.2f, %.2f, %.2f], Thumb2[%.2f, %.2f, %.2f], Thumb3[%.2f, %.2f, %.2f]",
                    frameCounter++,
                    leftThumb1.euler_degree[0], leftThumb1.euler_degree[1], leftThumb1.euler_degree[2],
                    leftThumb2.euler_degree[0], leftThumb2.euler_degree[1], leftThumb2.euler_degree[2],
                    leftThumb3.euler_degree[0], leftThumb3.euler_degree[1], leftThumb3.euler_degree[2]
                );

                printf("\r\n");
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