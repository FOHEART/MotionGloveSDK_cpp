#ifdef _WIN32
#include <windows.h>

#else
#include <unistd.h>
#endif
#include <stdio.h>

#include <string>
#include "motionGloveSDK.h"

static const char* HEADER_COMMENT =
    u8"/*******motionGloveSDK_example4*********/\n"
    u8"本程序用来测试左手中指三段骨骼的角度\n"
    u8"中指第1段的角度为相对于手背的角度\n"
    u8"中指第2段的角度为相对于第1段的角度\n"
    u8"中指第3段的角度为相对于第2段的角度\n"
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

            if (0 == MotionGloveSDK_GetGloveSkeletonsFrame(gloveName.c_str(), &KHHS32PosAttitude))
            {
                SingleSkeletonDef leftMiddle1 = KHHS32PosAttitude.skeletons[KHHS32_LeftHandMiddle1];
                SingleSkeletonDef leftMiddle2 = KHHS32PosAttitude.skeletons[KHHS32_LeftHandMiddle2];
                SingleSkeletonDef leftMiddle3 = KHHS32PosAttitude.skeletons[KHHS32_LeftHandMiddle3];

                printf("Left Middle Finger Angles: ");
                printf("Middle1[%.2f, %.2f, %.2f], ",
                    leftMiddle1.euler_degree[0],
                    leftMiddle1.euler_degree[1],
                    leftMiddle1.euler_degree[2]);
                printf("Middle2[%.2f, %.2f, %.2f], ",
                    leftMiddle2.euler_degree[0],
                    leftMiddle2.euler_degree[1],
                    leftMiddle2.euler_degree[2]);
                printf("Middle3[%.2f, %.2f, %.2f]",
                    leftMiddle3.euler_degree[0],
                    leftMiddle3.euler_degree[1],
                    leftMiddle3.euler_degree[2]);

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