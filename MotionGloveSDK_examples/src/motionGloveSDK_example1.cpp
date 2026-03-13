#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include <string>
#include "motionGloveSDK.h"

static const char* HEADER_COMMENT =
    u8"/*******motionGloveSDK_example1*********/\r\n"
    u8"用来测试MotionGloveSDK 的数据转发，默认使用UDP监听本地5001端口\r\n"
    u8"使用流程：1、打开MotionGlove软件，并且手套连接正常\r\n"
    u8"2、确保在软件菜单栏->设置->插件->数据转发中添加了127.0.0.1端口5000（软件默认已添加）\r\n"
    u8"如果运行正常，会打印出收到数据帧的帧序号，每秒60帧\r\n"
    u8"/*******************************************/\n";

/*
绑定本机的5001端口 开始读取数据
端口号要与MotionGlove中 设置->选项->插件->数据转发 中的端口号一致
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

    while (true)
    {
        /*
        接收到的数据包为60Hz
        */
        std::string gloveName = "Glove1";
        static int frameCounter = 0;
        if (MotionGloveSDK_isGloveNewFramePending(gloveName.c_str()))
        {
            printf("[%d]New frame received\n", frameCounter++);
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