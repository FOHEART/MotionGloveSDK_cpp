/**
 * @file  export_cmd.cpp
 * @brief `export` 命令实现：复用 run 的采集逻辑，把帧写成 JSON / CSV。
 * @ingroup mgsdk_cli
 */
#include "commands/export_cmd.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "commands/run_cmd.h"
#include "sdk_info.h"

namespace mgcli {

namespace {

/** @brief 目录是否存在（不是文件）。 */
bool directoryExists(const std::string& path)
{
	struct stat info;
	if (stat(path.c_str(), &info) != 0) return false;
	return (info.st_mode & S_IFDIR) != 0;
}

/** @brief 目录部分（末尾不带分隔符）；没有目录时返回空串。 */
std::string parentDirectory(const std::string& path)
{
	const std::string::size_type slash = path.find_last_of("/\\");
	return (std::string::npos == slash) ? std::string() : path.substr(0, slash);
}

std::string lowerExtension(const std::string& path)
{
	const std::string::size_type dot = path.find_last_of('.');
	if (std::string::npos == dot) return std::string();
	std::string ext = path.substr(dot + 1);
	for (size_t i = 0; i < ext.size(); ++i)
	{
		if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = static_cast<char>(ext[i] - 'A' + 'a');
	}
	return ext;
}

/** @brief 按扩展名推断格式；无法推断时返回空串。 */
std::string inferFormat(const std::string& path)
{
	const std::string ext = lowerExtension(path);
	if (ext == "json") return "json";
	if (ext == "csv") return "csv";
	return std::string();
}

std::string buildJsonFile(const CaptureResult& result, const CaptureOptions& options)
{
	std::ostringstream out;
	out << "{";
	out << "\"exportedAt\":" << jsonString(localTimeIso8601());
	out << ",\"cliVersion\":" << jsonString(cliVersionString());
	out << ",\"sdkVersion\":" << jsonString(sdkVersionString());
	out << ",\"port\":" << options.port;
	out << ",\"actor\":" << jsonString(options.actor);
	out << ",\"frameCount\":" << result.frames.size();
	out << ",\"layout\":"
	    << (result.frames.empty() ? std::string("null")
	                              : jsonNumber(static_cast<double>(result.frames[0].skeletons.size())));
	out << ",\"frames\":[";
	for (size_t i = 0; i < result.frames.size(); ++i)
	{
		if (i > 0) out << ",";
		out << frameToJson(result.frames[i], options);
	}
	out << "]}\n";
	return out.str();
}

void appendCsvRow(std::ostringstream& out, const CapturedFrame& frame, int index,
                  const CaptureOptions& options)
{
	const SingleSkeletonDef& skeleton = frame.skeletons[static_cast<size_t>(index)];
	const int format = static_cast<int>(frame.hdr.skeletonFormat);

	char buffer[256] = { 0 };
	out << frame.hdr.frameNumber << "," << frame.skeletons.size() << "," << index << ","
	    << skeletonName(format, index);

	if (options.fields.pos)
	{
		for (int i = 0; i < 3; ++i)
		{
			snprintf(buffer, sizeof(buffer), ",%.6f", static_cast<double>(skeleton.position_meter[i]));
			out << buffer;
		}
	}
	if (options.fields.euler)
	{
		for (int i = 0; i < 3; ++i)
		{
			snprintf(buffer, sizeof(buffer), ",%.6f", static_cast<double>(skeleton.euler_degree[i]));
			out << buffer;
		}
	}
	if (options.fields.quat)
	{
		for (int i = 0; i < 4; ++i)
		{
			snprintf(buffer, sizeof(buffer), ",%.6f", static_cast<double>(skeleton.quat_xyzw[i]));
			out << buffer;
		}
	}
	out << "\n";
}

std::string buildCsvFile(const CaptureResult& result, const CaptureOptions& options)
{
	std::ostringstream out;
	out << "frame_number,skeleton_count,skeleton_index,skeleton_name";
	if (options.fields.pos) out << ",pos_x,pos_y,pos_z";
	if (options.fields.euler) out << ",euler_x,euler_y,euler_z";
	if (options.fields.quat) out << ",quat_x,quat_y,quat_z,quat_w";
	out << "\n";

	for (size_t f = 0; f < result.frames.size(); ++f)
	{
		const CapturedFrame& frame = result.frames[f];
		const int format = static_cast<int>(frame.hdr.skeletonFormat);
		if (options.joints.empty())
		{
			for (int i = 0; i < static_cast<int>(frame.skeletons.size()); ++i)
			{
				appendCsvRow(out, frame, i, options);
			}
		}
		else
		{
			for (size_t j = 0; j < options.joints.size(); ++j)
			{
				const int index = resolveJointIndex(options.joints[j], format);
				if (index >= 0 && index < static_cast<int>(frame.skeletons.size()))
				{
					appendCsvRow(out, frame, index, options);
				}
			}
		}
	}
	return out.str();
}

std::string exportDataJson(const CaptureResult& result, const CaptureOptions& options,
                           const std::string& output, const std::string& format,
                           const std::string& error)
{
	std::ostringstream out;
	out << "{";
	out << "\"output\":" << jsonString(output);
	out << ",\"format\":" << jsonString(format);
	out << ",\"port\":" << options.port;
	out << ",\"actor\":" << jsonString(options.actor);
	out << ",\"requestedFrames\":" << options.frames;
	out << ",\"frameCount\":" << result.frames.size();
	out << ",\"elapsedMs\":" << result.elapsedMs;
	out << ",\"listenRet\":" << result.listenRet;
	if (!error.empty()) out << ",\"error\":" << jsonString(error);
	out << "}";
	return out.str();
}

} // namespace

CommandResult exportCommand(const std::vector<std::string>& tokens)
{
	Args args(tokens);
	std::string error;

	if (!args.validate(std::vector<std::string>(),
	                   std::vector<std::string>{ "--port", "--actor", "--frames", "--timeout-ms",
	                                             "--poll-ms", "--joint", "--fields", "--output",
	                                             "--format" },
	                   &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	if (!args.has("--output") || args.value("--output").empty())
	{
		return CommandResult::fail(KUsage, "missing required option: --output <path.json|path.csv>");
	}
	const std::string output = args.value("--output");

	std::string format = args.value("--format");
	if (!format.empty() && format != "json" && format != "csv")
	{
		return CommandResult::fail(KUsage, "invalid --format: " + format + " (allowed: json,csv)");
	}
	if (format.empty())
	{
		format = inferFormat(output);
		if (format.empty())
		{
			return CommandResult::fail(
				KUsage, "cannot infer export format from '" + output
				+ "'; add --format json|csv or use a .json/.csv extension");
		}
	}

	const std::string directory = parentDirectory(output);
	if (!directory.empty() && !directoryExists(directory))
	{
		return CommandResult::fail(KIoError, "output directory does not exist: " + directory);
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
	if (KOk != result.code)
	{
		return CommandResult::fail(result.code, result.message,
		                           exportDataJson(result, options, output, format, result.message),
		                           captureHumanText(result, options));
	}

	const std::string content = ("json" == format)
		? buildJsonFile(result, options)
		: buildCsvFile(result, options);

	std::string writeError;
	if (!writeWholeFile(output, content, &writeError))
	{
		return CommandResult::fail(KIoError, writeError,
		                           exportDataJson(result, options, output, format, writeError));
	}

	std::ostringstream data;
	data << "{";
	data << "\"output\":" << jsonString(output);
	data << ",\"format\":" << jsonString(format);
	data << ",\"bytes\":" << content.size();
	data << ",\"port\":" << options.port;
	data << ",\"actor\":" << jsonString(options.actor);
	data << ",\"frameCount\":" << result.frames.size();
	data << ",\"layout\":"
	     << jsonNumber(static_cast<double>(result.frames[0].skeletons.size()));
	data << ",\"elapsedMs\":" << result.elapsedMs;
	data << "}";

	std::ostringstream human;
	human << "exported " << result.frames.size() << " frame(s) to " << output
	      << " (" << format << ", " << content.size() << " bytes)";

	CommandResult command = CommandResult::succeed(data.str(), human.str());
	command.message = result.message;
	return command;
}

} // namespace mgcli
