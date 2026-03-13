#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <string>
#include "motionGloveSDK.h"

/**
 * @file motionGloveSDK_example5.cpp
 * @brief
 * @brief_cn: 用于循环测试UDP端口绑定与释放稳定性的示例程序。
 * @brief_en: Example program to repeatedly test stability of UDP port bind and release.
 *
 * 流程/Procedure:
 *  - 绑定端口 -> 接收5秒 -> 关闭端口 -> 等待1秒 -> 重复
 *  - Bind port -> receive for 5 seconds -> close port -> wait 1 second -> repeat
 */

/**
 * @brief_cn: 默认监听UDP端口号。
 * @brief_en: Default UDP port to bind and listen on.
 */
const int rxPortDefault = 5001;

/**
 * @brief_cn: 接收持续时间，单位毫秒。
 * @brief_en: Duration to receive data in milliseconds.
 */
const int receiveDurationMs = 5000;

/**
 * @brief_cn: 关闭后冷却等待时间，单位毫秒。
 * @brief_en: Cooldown time after closing the port in milliseconds.
 */
const int cooldownMs = 1000;

/**
 * @brief_cn: 程序入口。
 * @brief_en: Program entry point.
 *
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 返回0表示正常退出
 */
int main(int argc, char* argv[])
{
#ifdef _WIN32
    /*
     * @brief_cn: 设置控制台输出为UTF-8，便于打印中文。
     * @brief_en: Set console encoding to UTF-8 for correct display of Chinese text.
     */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /**
     * @brief_cn: 程序说明头。
     * @brief_en: Header comment printed at start.
     */
    static const char* HEADER_COMMENT =
        u8"/*******motionGloveSDK_example5*********/\n"
        u8"本程序用于循环测试UDP端口绑定与释放稳定性\n"
        u8"流程：绑定端口->接收5秒->关闭端口->等待1秒->重复\n"
        u8"Example: bind -> receive 5s -> close -> wait 1s -> repeat\n"
        u8"/*******************************************/\n";

    printf("%s", HEADER_COMMENT);

    int loopCounter = 0;
    while (true)
    {
        loopCounter++;
        printf("\n[Test %d] Try bind UDP 127.0.0.1:%d\n", loopCounter, rxPortDefault);

        /*
         * @brief_cn: 绑定UDP端口并开始接收数据。
         * @brief_en: Bind UDP port and start receiving data.
         */
        int listenRet = MotionGloveSDK_ListenUDPPort(rxPortDefault);
        if (listenRet != 0)
        {
            printf("[Test %d] Bind failed, code=%d\n", loopCounter, listenRet);

            /*
             * @brief_cn: 绑定失败时等待冷却时间后重试。
             * @brief_en: On bind failure wait for cooldown then retry.
             */
#ifdef _WIN32
            Sleep(cooldownMs);
#else
            usleep(cooldownMs * 1000);
#endif
            continue;
        }

        printf("[Test %d] Bind success, receiving for %d ms...\n", loopCounter, receiveDurationMs);

        /*
         * @brief_cn: 在接收期间循环检查是否收到新帧并计数。
         * @brief_en: During receive period poll for new frames and count them.
         */
        std::string gloveName = "Glove1";
        int elapsedMs = 0;
        int frameCounter = 0;
        while (elapsedMs < receiveDurationMs)
        {
            if (MotionGloveSDK_isGloveNewFramePending(gloveName.c_str()))
            {
                frameCounter++;
                MotionGloveSDK_resetGloveNewFramePending(gloveName.c_str());
            }

            /*
             * @brief_cn: 这里睡眠10ms以降低CPU占用。
             * @brief_en: Sleep for 10 ms to reduce CPU load.
             */
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
            elapsedMs += 10;
        }

        printf("[Test %d] Receive done, total frames=%d\n", loopCounter, frameCounter);

        /*
         * @brief_cn: 关闭UDP端口并释放资源。
         * @brief_en: Close UDP port and release resources.
         */
        int closeRet = MotionGloveSDK_CloseUDPPort();
        printf("[Test %d] CloseUDPPort ret=%d\n", loopCounter, closeRet);

        /*
         * @brief_cn: 关闭后等待冷却时间，再进入下一轮测试。
         * @brief_en: After closing wait for cooldown before next iteration.
         */
        printf("[Test %d] Cooldown %d ms...\n", loopCounter, cooldownMs);
#ifdef _WIN32
        Sleep(cooldownMs);
#else
        usleep(cooldownMs * 1000);
#endif
    }

    return 0;
}
