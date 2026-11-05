/**
 * @file  status_cmd.cpp
 * @brief `status` / `version` 命令实现。
 * @ingroup mgsdk_cli
 */
#include "commands/status_cmd.h"

#include <sstream>

#include "commands/run_cmd.h"
#include "sdk_info.h"
#include "session.h"

namespace mgcli {

namespace {

/** @brief 平台标签，便于 Agent 判断脚本分支。 */
const char* platformLabel()
{
#ifdef _WIN32
	return "windows";
#else
	return "linux";
#endif
}

} // namespace

CommandResult statusCommand(const std::vector<std::string>& tokens)
{
	Args args(tokens);
	std::string error;
	if (!args.validate(std::vector<std::string>(),
	                   std::vector<std::string>{ "--port", "--actor" }, &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	std::string warning;
	const Session session = loadSession(&warning);

	int port = kDefaultPort;
	std::string actor = kDefaultActor;
	if (!resolvePortAndActor(args, &port, &actor, &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	std::ostringstream data;
	data << "{";
	data << "\"cliVersion\":" << jsonString(cliVersionString());
	data << ",\"sdkVersion\":" << jsonString(sdkVersionString());
	data << ",\"sdkVersionRaw\":" << sdkVersionRaw();
	data << ",\"exe\":" << jsonString(executablePath());
	data << ",\"platform\":" << jsonString(platformLabel());
	data << ",\"transport\":" << jsonString("udp-csv");
	data << ",\"defaults\":{\"port\":" << kDefaultPort
	     << ",\"actor\":" << jsonString(kDefaultActor)
	     << ",\"timeoutMs\":" << kDefaultTimeoutMs << "}";
	data << ",\"session\":{\"path\":" << jsonString(session.path)
	     << ",\"exists\":" << jsonBool(session.exists)
	     << ",\"port\":" << (session.hasPort ? jsonNumber(session.port) : std::string("null"))
	     << ",\"actor\":" << (session.hasActor ? jsonString(session.actor) : std::string("null"))
	     << "}";
	data << ",\"resolved\":{\"port\":" << port
	     << ",\"actor\":" << jsonString(actor) << "}";
	data << "}";

	std::ostringstream human;
	human << "motionGloveSDK_cli " << cliVersionString() << " (SDK " << sdkVersionString() << ")\n";
	human << "exe: " << executablePath() << "\n";
	human << "platform: " << platformLabel() << "\n";
	human << "defaults: port=" << kDefaultPort << " actor=" << kDefaultActor
	      << " timeout=" << kDefaultTimeoutMs << "ms\n";
	human << "session: " << session.path;
	if (!session.exists)
	{
		human << " (absent)";
	}
	else
	{
		human << " (port=";
		if (session.hasPort) human << session.port; else human << "-";
		human << " actor=";
		if (session.hasActor) human << session.actor; else human << "-";
		human << ")";
	}
	human << "\nresolved: port=" << port << " actor=" << actor;

	CommandResult result = CommandResult::succeed(data.str(), human.str());
	result.message = warning;
	return result;
}

CommandResult versionCommand(const std::vector<std::string>& tokens)
{
	Args args(tokens);
	std::string error;
	if (!args.validate(std::vector<std::string>(), std::vector<std::string>(), &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	std::ostringstream data;
	data << "{\"cliVersion\":" << jsonString(cliVersionString())
	     << ",\"sdkVersion\":" << jsonString(sdkVersionString())
	     << ",\"sdkVersionRaw\":" << sdkVersionRaw() << "}";

	std::ostringstream human;
	human << "motionGloveSDK_cli " << cliVersionString()
	      << " (MotionGloveSDK " << sdkVersionString() << ")";

	return CommandResult::succeed(data.str(), human.str());
}

} // namespace mgcli
