/**
 * @file  motionGloveSDK_example1.cpp
 * @brief 示例 1：绑定 UDP 端口并持续接收手套数据流，按统一行首打印新帧到达。
 * @ingroup mgsdk_examples
 *
 * @details
 * 演示 SDK 最小使用闭环：ListenUDPPort -> 循环查询新帧（isGloveNewFramePending）
 * -> 取帧（GetSkeletons42Frame）-> 打印统一行首（帧序号 + 骨骼段数）。
 * 默认绑定 127.0.0.1:5001，数据源由 MotionVenus 软件按转发规则推送。
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
    u8"/*******motionGloveSDK_example1*********/\r\n"
    u8"用来测试MotionGloveSDK 的数据转发，默认使用UDP监听本地5001端口\r\n"
    u8"使用流程：1、打开MotionGlove软件，并且手套连接正常\r\n"
    u8"2、确保在软件菜单栏->设置->插件->数据转发中添加了127.0.0.1端口5000（软件默认已添加）\r\n"
    u8"如果运行正常，会打印出收到数据帧的帧序号，每秒60帧\r\n"
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

    /*
 	如果不想每次程序打开都打印使用说明，注释掉下面这行代码即可
    */
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

    /* 兼容两种数据流格式：32 段与 42 段各有独立的容器与取帧接口 */
    KHHS42PosAttitude_TypeDef KHHS42PosAttitude;
    KHHS32PosAttitude_TypeDef KHHS32PosAttitude;
    while (true)
    {
        /*
        接收到的数据包为60Hz
        */
        std::string gloveName = "Glove1";
        if (MotionGloveSDK_isGloveNewFramePending(gloveName.c_str()))
        {
            GloveStreamSkeletonFormat_TypeDef frameType = GloveStreamFormat_Skeleton42;
            (void)MotionGloveSDK_GetSkeletonsFrameType(gloveName.c_str(), &frameType);
            const bool isLayout42 = (GloveStreamFormat_Skeleton42 == frameType);
            const int getRet = isLayout42
                ? MotionGloveSDK_GetSkeletons42Frame(gloveName.c_str(), &KHHS42PosAttitude)
                : MotionGloveSDK_GetSkeletons32Frame(gloveName.c_str(), &KHHS32PosAttitude);

            if (0 == getRet)
            {
                PrintGloveFrameHeader(isLayout42 ? KHHS42PosAttitude.hdr : KHHS32PosAttitude.hdr);
                printf("New frame received\n");
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