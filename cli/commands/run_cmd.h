/**
 * @file  run_cmd.h
 * @brief `run` 命令：绑定 UDP、采集手套帧并按统一信封输出（export 复用同一采集逻辑）。
 * @ingroup mgsdk_cli
 *
 * @details
 * 采集是"每进程一次"的无状态流程：绑定端口 -> 轮询新帧 -> 取帧 -> 关闭端口。
 * 不依赖任何 GUI/窗口，可在无头环境与 CI 中运行。
 */
#pragma once

#include <string>
#include <vector>

#include "cli_parser.h"
#include "motionGloveSDK.h"

namespace mgcli {

/** @brief 默认监听端口（与 MotionVenus 默认转发端口一致）。 */
const int kDefaultPort = 5001;

/** @brief 默认数据流名称前缀（协议中演员名以 "Glove" 开头，按前缀匹配）。 */
const char* const kDefaultActor = "Glove1";

/** @brief 默认超时（毫秒）。 */
const int kDefaultTimeoutMs = 3000;

/** @brief 默认轮询间隔（毫秒）。 */
const int kDefaultPollMs = 10;

/** @brief 输出字段选择。 */
struct FieldSelection {
	bool pos;
	bool euler;
	bool quat;

	FieldSelection() : pos(true), euler(true), quat(true) {}
};

/** @brief 一次采集的参数（已完成端口/数据流名解析与校验）。 */
struct CaptureOptions {
	int port;
	std::string actor;
	int frames;     ///< 采集帧数；0 表示一直采到超时
	int timeoutMs;
	int pollMs;
	std::vector<std::string> joints; ///< 骨骼过滤（名称或序号），空表示全部
	FieldSelection fields;

	CaptureOptions();
};

/** @brief 采集到的一帧（拷贝自 SDK 缓存，脱离 SDK 生命周期）。 */
struct CapturedFrame {
	StreamHeader hdr;
	std::vector<SingleSkeletonDef> skeletons;
	std::string remoteIp;
	unsigned int remotePort;

	CapturedFrame() : remotePort(0) {}
};

/** @brief 采集结果。 */
struct CaptureResult {
	int code;            ///< 0 / KSdkError / KTimeout / KFrameError
	std::string message; ///< 失败或部分成功说明
	int listenRet;       ///< MotionGloveSDK_ListenUDPPort 的返回值
	long long elapsedMs; ///< 实际耗时
	int frameErrors;     ///< 取帧失败次数
	std::vector<CapturedFrame> frames;

	CaptureResult();
};

/** @brief 解析端口与数据流名：命令行 > 会话文件 > 内置默认值。 */
bool resolvePortAndActor(const Args& args, int* port, std::string* actor, std::string* error);

/** @brief 解析 run/export 共用的采集选项（端口、数据流名需先解析好）。 */
bool applyCaptureArgs(const Args& args, int port, const std::string& actor,
                      CaptureOptions* options, std::string* error);

/** @brief 执行采集：绑定端口、轮询取帧、关闭端口。 */
CaptureResult captureFrames(const CaptureOptions& options);

/** @brief 骨骼名称：按帧格式取 42 段或 32 段名称表。 */
std::string skeletonName(int format, int index);

/** @brief 把骨骼文本（名称或序号）解析成下标；无法解析返回 -1。 */
int resolveJointIndex(const std::string& text, int format);

/** @brief 单帧的 JSON 对象文本（按 fields/joints 过滤）。 */
std::string frameToJson(const CapturedFrame& frame, const CaptureOptions& options);

/** @brief 单帧的人类可读文本（多行，末行不带换行）。 */
std::string frameToHuman(const CapturedFrame& frame, const CaptureOptions& options);

/** @brief 采集结果的数据部分 JSON（frames 数组 + 元信息）。 */
std::string captureDataJson(const CaptureResult& result, const CaptureOptions& options);

/** @brief 采集结果的人类可读文本（逐帧 + 汇总）。 */
std::string captureHumanText(const CaptureResult& result, const CaptureOptions& options);

/** @brief `run` 命令入口（tokens 为命令名之后的参数）。 */
CommandResult runCommand(const std::vector<std::string>& tokens);

} // namespace mgcli
