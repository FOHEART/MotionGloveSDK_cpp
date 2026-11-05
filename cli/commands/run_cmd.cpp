/**
 * @file  run_cmd.cpp
 * @brief `run` 命令实现：UDP 采集手套帧，输出统一 JSON 信封或人类可读文本。
 * @ingroup mgsdk_cli
 */
#include "commands/run_cmd.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>

#include "session.h"

namespace mgcli {

namespace {

typedef std::chrono::steady_clock Clock;

long long elapsedMs(const Clock::time_point& start)
{
	const Clock::duration span = Clock::now() - start;
	return static_cast<long long>(
		std::chrono::duration_cast<std::chrono::milliseconds>(span).count());
}

std::string intToString(int value)
{
	char buffer[32] = { 0 };
	snprintf(buffer, sizeof(buffer), "%d", value);
	return buffer;
}

const char* suitTypeLabel(SuitType type)
{
	switch (type)
	{
	case FOHEART_X: return "FOHEART_X";
	case FOHEART_MAGIC: return "FOHEART_MAGIC";
	default: return "UNDEFINED";
	}
}

const char* streamFormatLabel(StreamFormat_TypeDef format)
{
	return (StreamFormat_String == format) ? "string" : "binary";
}

const char* positionLabel(SkeletonPosition_TypeDef value)
{
	return (SkeletonPosition_Meter == value) ? "meter" : "none";
}

const char* attitudeLabel(SkeletonAttitude_TypeDef value)
{
	switch (value)
	{
	case SkeletonAttitude_Euler: return "euler";
	case SkeletonAttitude_Quat: return "quat";
	default: return "none";
	}
}

const char* coordinateLabel(SkeletonCoordinate_TypeDef value)
{
	return (SkeletonCoordinate_Global == value) ? "global" : "relative";
}

std::string channelOrderLabel(MVSDK_ChannelOrder order)
{
	const int index = static_cast<int>(order);
	if (index >= 0 && index < MVSDK_CHANNEL_ORDER_MAX)
	{
		return MVSDK_ChannelOrder_StringDesc[index];
	}
	return "?";
}

/** @brief 帧头存储的是网络字节序端口；x86/ARM 小端下按字节交换还原。 */
unsigned int hostPort(unsigned int networkPort)
{
	return ((networkPort & 0xFFu) << 8) | ((networkPort >> 8) & 0xFFu);
}

std::string ipToString(const uint8_t ip[4])
{
	char buffer[32] = { 0 };
	snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
	         static_cast<unsigned int>(ip[0]), static_cast<unsigned int>(ip[1]),
	         static_cast<unsigned int>(ip[2]), static_cast<unsigned int>(ip[3]));
	return buffer;
}

/** @brief 本次输出涉及的骨骼下标（保持过滤顺序）。 */
std::vector<int> selectedJoints(const CapturedFrame& frame, const CaptureOptions& options)
{
	std::vector<int> indices;
	const int count = static_cast<int>(frame.skeletons.size());
	if (options.joints.empty())
	{
		for (int i = 0; i < count; ++i) indices.push_back(i);
		return indices;
	}
	for (size_t i = 0; i < options.joints.size(); ++i)
	{
		const int index = resolveJointIndex(options.joints[i], frame.hdr.skeletonFormat);
		if (index >= 0 && index < count)
		{
			indices.push_back(index);
		}
	}
	return indices;
}

std::string floatsJson(const float* values, int count)
{
	std::ostringstream out;
	out << "[";
	for (int i = 0; i < count; ++i)
	{
		if (i > 0) out << ",";
		out << jsonNumber(static_cast<double>(values[i]));
	}
	out << "]";
	return out.str();
}

std::string floatsHuman(const float* values, int count, int precision)
{
	std::ostringstream out;
	out << "(";
	for (int i = 0; i < count; ++i)
	{
		if (i > 0) out << ", ";
		out << formatFixed(static_cast<double>(values[i]), precision);
	}
	out << ")";
	return out.str();
}

/** @brief 采集期间按当前帧格式取一帧并拷贝到 CapturedFrame。 */
bool fetchFrame(const std::string& actor, CapturedFrame* frame, std::string* error, int* formatOut)
{
	GloveStreamSkeletonFormat_TypeDef format = GloveStreamFormat_Skeleton42;
	const int formatRet = MotionGloveSDK_GetSkeletonsFrameType(actor.c_str(), &format);
	if (formatRet != 0)
	{
		*error = "GetSkeletonsFrameType failed (ret=" + intToString(formatRet) + ")";
		return false;
	}

	if (GloveStreamFormat_Skeleton42 == format)
	{
		KHHS42PosAttitude_TypeDef value;
		const int ret = MotionGloveSDK_GetSkeletons42Frame(actor.c_str(), &value);
		if (ret != 0)
		{
			*error = "GetSkeletons42Frame failed (ret=" + intToString(ret) + ")";
			return false;
		}
		frame->hdr = value.hdr;
		frame->skeletons.assign(value.skeletons, value.skeletons + KHHS42_SkeletonCount);
		frame->remoteIp = ipToString(value.remoteIP);
		frame->remotePort = hostPort(value.remotePort);
		*formatOut = 42;
		return true;
	}

	if (GloveStreamFormat_Skeleton32 == format)
	{
		KHHS32PosAttitude_TypeDef value;
		const int ret = MotionGloveSDK_GetSkeletons32Frame(actor.c_str(), &value);
		if (ret != 0)
		{
			*error = "GetSkeletons32Frame failed (ret=" + intToString(ret) + ")";
			return false;
		}
		frame->hdr = value.hdr;
		frame->skeletons.assign(value.skeletons, value.skeletons + KHHS32_SkeletonCount);
		frame->remoteIp = ipToString(value.remoteIP);
		frame->remotePort = hostPort(value.remotePort);
		*formatOut = 32;
		return true;
	}

	*error = "unsupported skeleton layout (OpenXR is not implemented yet)";
	return false;
}

} // namespace

CaptureOptions::CaptureOptions()
	: port(kDefaultPort), actor(kDefaultActor), frames(1),
	  timeoutMs(kDefaultTimeoutMs), pollMs(kDefaultPollMs) {}

CaptureResult::CaptureResult()
	: code(KOk), listenRet(0), elapsedMs(0), frameErrors(0) {}

std::string skeletonName(int format, int index)
{
	if (GloveStreamFormat_Skeleton32 == format)
	{
		if (index >= 0 && index < KHHS32_SkeletonCount) return kinemHumanHandsSkeleton32[index];
		return "?";
	}
	if (index >= 0 && index < KHHS42_SkeletonCount) return kinemHumanHandsSkeleton42[index];
	return "?";
}

int resolveJointIndex(const std::string& text, int format)
{
	const bool is32 = (GloveStreamFormat_Skeleton32 == format);
	const int count = is32 ? KHHS32_SkeletonCount : KHHS42_SkeletonCount;

	if (isAllDigits(text))
	{
		const int index = atoi(text.c_str());
		return (index >= 0 && index < count) ? index : -1;
	}
	for (int i = 0; i < count; ++i)
	{
		const std::string name = is32 ? kinemHumanHandsSkeleton32[i] : kinemHumanHandsSkeleton42[i];
		if (equalsIgnoreCase(text, name)) return i;
	}
	return -1;
}

bool resolvePortAndActor(const Args& args, int* port, std::string* actor, std::string* error)
{
	std::string warning;
	const Session session = loadSession(&warning);

	int resolvedPort = session.hasPort ? session.port : kDefaultPort;
	std::string resolvedActor = session.hasActor ? session.actor : kDefaultActor;

	if (args.has("--port") && !args.getInt("--port", resolvedPort, &resolvedPort, error))
	{
		return false;
	}
	if (args.has("--actor"))
	{
		resolvedActor = args.value("--actor");
	}

	if (resolvedPort < 1024 || resolvedPort > 65535)
	{
		*error = "--port must be in [1024, 65535], got " + intToString(resolvedPort);
		return false;
	}
	if (resolvedActor.empty() || resolvedActor.size() > 63)
	{
		*error = "--actor must be 1..63 bytes, got " + intToString(static_cast<int>(resolvedActor.size())) + " bytes";
		return false;
	}

	*port = resolvedPort;
	*actor = resolvedActor;
	return true;
}

bool applyCaptureArgs(const Args& args, int port, const std::string& actor,
                      CaptureOptions* options, std::string* error)
{
	options->port = port;
	options->actor = actor;

	if (!args.getInt("--frames", options->frames, &options->frames, error)) return false;
	if (!args.getInt("--timeout-ms", options->timeoutMs, &options->timeoutMs, error)) return false;
	if (!args.getInt("--poll-ms", options->pollMs, &options->pollMs, error)) return false;

	if (options->frames < 0)
	{
		*error = "--frames must be >= 0 (0 = until timeout)";
		return false;
	}
	if (options->timeoutMs < 1)
	{
		*error = "--timeout-ms must be >= 1";
		return false;
	}
	if (options->pollMs < 1 || options->pollMs > 1000)
	{
		*error = "--poll-ms must be in [1, 1000]";
		return false;
	}

	options->joints.clear();
	const std::vector<std::string> jointArgs = args.values("--joint");
	for (size_t i = 0; i < jointArgs.size(); ++i)
	{
		const std::vector<std::string> items = splitCsv(jointArgs[i]);
		for (size_t k = 0; k < items.size(); ++k)
		{
			const std::string& item = items[k];
			if (isAllDigits(item))
			{
				const int index = atoi(item.c_str());
				if (index < 0 || index >= KHHS42_SkeletonCount)
				{
					*error = "joint index out of range: " + item;
					return false;
				}
				options->joints.push_back(item);
				continue;
			}
			if (resolveJointIndex(item, GloveStreamFormat_Skeleton42) < 0
			    && resolveJointIndex(item, GloveStreamFormat_Skeleton32) < 0)
			{
				*error = "unknown joint: " + item;
				return false;
			}
			options->joints.push_back(item);
		}
	}

	if (args.has("--fields"))
	{
		const std::vector<std::string> fields = splitCsv(args.value("--fields"));
		if (fields.empty())
		{
			*error = "--fields must be a comma separated subset of pos,euler,quat";
			return false;
		}
		options->fields.pos = false;
		options->fields.euler = false;
		options->fields.quat = false;
		for (size_t i = 0; i < fields.size(); ++i)
		{
			if (fields[i] == "pos") options->fields.pos = true;
			else if (fields[i] == "euler") options->fields.euler = true;
			else if (fields[i] == "quat") options->fields.quat = true;
			else
			{
				*error = "unknown field: " + fields[i] + " (allowed: pos,euler,quat)";
				return false;
			}
		}
	}
	return true;
}

CaptureResult captureFrames(const CaptureOptions& options)
{
	CaptureResult result;
	const Clock::time_point start = Clock::now();

	result.listenRet = MotionGloveSDK_ListenUDPPort(options.port);
	if (result.listenRet != 0)
	{
		result.code = KSdkError;
		result.message = "bind UDP port " + intToString(options.port) + " failed (ret="
		                 + intToString(result.listenRet)
		                 + "); port may be in use (netstat -ano -p UDP | findstr :"
		                 + intToString(options.port) + ") or blocked by firewall";
		result.elapsedMs = elapsedMs(start);
		return result;
	}

	std::string lastFrameError;
	while (true)
	{
		if (MotionGloveSDK_isGloveNewFramePending(options.actor.c_str()))
		{
			CapturedFrame frame;
			int format = 42;
			if (fetchFrame(options.actor, &frame, &lastFrameError, &format))
			{
				result.frames.push_back(frame);
			}
			else
			{
				++result.frameErrors;
			}
			/* 新帧标记必须复位，否则同一帧会被反复取走 */
			MotionGloveSDK_resetGloveNewFramePending(options.actor.c_str());
		}

		if (options.frames > 0 && static_cast<int>(result.frames.size()) >= options.frames)
		{
			break;
		}

		result.elapsedMs = elapsedMs(start);
		if (result.elapsedMs >= options.timeoutMs)
		{
			break;
		}

		int sleepMs = options.pollMs;
		const long long remaining = options.timeoutMs - result.elapsedMs;
		if (remaining < sleepMs) sleepMs = static_cast<int>(remaining);
		if (sleepMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
	}

	MotionGloveSDK_CloseUDPPort();
	result.elapsedMs = elapsedMs(start);

	if (result.frames.empty())
	{
		if (result.frameErrors > 0)
		{
			result.code = KFrameError;
			result.message = "no readable frame from stream '" + options.actor + "' after "
			                 + intToString(result.frameErrors) + " attempt(s): " + lastFrameError;
		}
		else
		{
			result.code = KTimeout;
			result.message = "no frames received within " + intToString(options.timeoutMs)
			                 + " ms on UDP " + intToString(options.port) + " (actor '"
			                 + options.actor + "'); check MotionVenus data forwarding and firewall";
		}
		return result;
	}

	result.code = KOk;
	if (options.frames > 0 && static_cast<int>(result.frames.size()) < options.frames)
	{
		result.message = "timeout: captured " + intToString(static_cast<int>(result.frames.size()))
		                 + " of " + intToString(options.frames) + " requested frames in "
		                 + intToString(static_cast<int>(result.elapsedMs)) + " ms";
	}
	return result;
}

std::string frameToJson(const CapturedFrame& frame, const CaptureOptions& options)
{
	const int format = static_cast<int>(frame.hdr.skeletonFormat);
	const std::vector<int> joints = selectedJoints(frame, options);

	std::ostringstream out;
	out << "{";
	out << "\"frameNumber\":" << frame.hdr.frameNumber;
	/* layout 输出"骨骼段数"（32/42），枚举值 0/1 对 Agent 没有意义 */
	out << ",\"layout\":" << frame.skeletons.size();
	out << ",\"skeletonCount\":" << frame.skeletons.size();
	out << ",\"protocolVersion\":" << frame.hdr.protocolVersion;
	out << ",\"suitNumber\":" << frame.hdr.suitNumber;
	out << ",\"suitType\":" << jsonString(suitTypeLabel(frame.hdr.suitType));
	out << ",\"streamFormat\":" << jsonString(streamFormatLabel(frame.hdr.streamFormat));
	out << ",\"position\":" << jsonString(positionLabel(frame.hdr.skeletonPosition));
	out << ",\"attitude\":" << jsonString(attitudeLabel(frame.hdr.skeletonAttitude));
	out << ",\"coordinate\":" << jsonString(coordinateLabel(frame.hdr.skeletonCoordinate));
	out << ",\"channelOrder\":" << jsonString(channelOrderLabel(frame.hdr.channelOrder));
	out << ",\"leftHandGesture\":" << static_cast<int>(frame.hdr.leftHandGesture);
	out << ",\"rightHandGesture\":" << static_cast<int>(frame.hdr.rightHandGesture);
	out << ",\"remoteIp\":" << jsonString(frame.remoteIp);
	out << ",\"remotePort\":" << frame.remotePort;
	out << ",\"skeletons\":[";

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const int index = joints[i];
		const SingleSkeletonDef& skeleton = frame.skeletons[static_cast<size_t>(index)];
		if (i > 0) out << ",";
		out << "{\"index\":" << index
		    << ",\"name\":" << jsonString(skeletonName(format, index));
		if (options.fields.pos) out << ",\"pos\":" << floatsJson(skeleton.position_meter, 3);
		if (options.fields.euler) out << ",\"euler\":" << floatsJson(skeleton.euler_degree, 3);
		if (options.fields.quat) out << ",\"quat\":" << floatsJson(skeleton.quat_xyzw, 4);
		out << "}";
	}
	out << "]}";
	return out.str();
}

std::string frameToHuman(const CapturedFrame& frame, const CaptureOptions& options)
{
	const int format = static_cast<int>(frame.hdr.skeletonFormat);
	const std::vector<int> joints = selectedJoints(frame, options);

	std::ostringstream out;
	for (size_t i = 0; i < joints.size(); ++i)
	{
		const int index = joints[i];
		const SingleSkeletonDef& skeleton = frame.skeletons[static_cast<size_t>(index)];
		out << "[" << frame.hdr.frameNumber << "][" << frame.skeletons.size() << "] "
		    << skeletonName(format, index);
		if (options.fields.pos) out << " pos=" << floatsHuman(skeleton.position_meter, 3, 4);
		if (options.fields.euler) out << " euler=" << floatsHuman(skeleton.euler_degree, 3, 2);
		if (options.fields.quat) out << " quat=" << floatsHuman(skeleton.quat_xyzw, 4, 4);
		if (i + 1 < joints.size()) out << "\n";
	}
	return out.str();
}

std::string captureDataJson(const CaptureResult& result, const CaptureOptions& options)
{
	const int captured = static_cast<int>(result.frames.size());
	const bool complete = captured > 0
		&& (options.frames == 0 || captured >= options.frames);

	std::ostringstream out;
	out << "{";
	out << "\"port\":" << options.port;
	out << ",\"actor\":" << jsonString(options.actor);
	out << ",\"requestedFrames\":" << options.frames;
	out << ",\"frameCount\":" << captured;
	out << ",\"complete\":" << jsonBool(complete);
	out << ",\"elapsedMs\":" << result.elapsedMs;
	out << ",\"timeoutMs\":" << options.timeoutMs;
	out << ",\"pollMs\":" << options.pollMs;
	out << ",\"listenRet\":" << result.listenRet;
	out << ",\"frameErrors\":" << result.frameErrors;
	out << ",\"layout\":"
	    << (result.frames.empty() ? std::string("null")
	                              : jsonNumber(static_cast<double>(result.frames[0].skeletons.size())));
	out << ",\"frames\":[";
	for (size_t i = 0; i < result.frames.size(); ++i)
	{
		if (i > 0) out << ",";
		out << frameToJson(result.frames[i], options);
	}
	out << "]}";
	return out.str();
}

std::string captureHumanText(const CaptureResult& result, const CaptureOptions& options)
{
	std::ostringstream out;
	for (size_t i = 0; i < result.frames.size(); ++i)
	{
		out << frameToHuman(result.frames[i], options) << "\n";
	}

	const int captured = static_cast<int>(result.frames.size());
	double rate = 0.0;
	if (result.elapsedMs > 0)
	{
		rate = captured * 1000.0 / static_cast<double>(result.elapsedMs);
	}
	out << "frames=" << captured;
	if (options.frames > 0) out << "/" << options.frames;
	out << " elapsed=" << result.elapsedMs << "ms rate=" << formatFixed(rate, 1) << "Hz";
	return out.str();
}

CommandResult runCommand(const std::vector<std::string>& tokens)
{
	Args args(tokens);
	std::string error;

	if (!args.validate(std::vector<std::string>(),
	                   std::vector<std::string>{ "--port", "--actor", "--frames", "--timeout-ms",
	                                             "--poll-ms", "--joint", "--fields" },
	                   &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	int port = kDefaultPort;
	std::string actor = kDefaultActor;
	if (!resolvePortAndActor(args, &port, &actor, &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	CaptureOptions options;
	if (!applyCaptureArgs(args, port, actor, &options, &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	const CaptureResult result = captureFrames(options);
	const std::string data = captureDataJson(result, options);
	const std::string human = captureHumanText(result, options);
	if (KOk != result.code)
	{
		return CommandResult::fail(result.code, result.message, data, human);
	}

	CommandResult command = CommandResult::succeed(data, human);
	command.message = result.message;
	return command;
}

} // namespace mgcli
