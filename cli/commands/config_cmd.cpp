/**
 * @file  config_cmd.cpp
 * @brief `config` 命令实现：会话默认值的读写与清除。
 * @ingroup mgsdk_cli
 */
#include "commands/config_cmd.h"

#include <cstdlib>
#include <sstream>

#include "commands/run_cmd.h"
#include "session.h"

namespace mgcli {

namespace {

/** @brief 输出会话内容的公共 JSON 片段。 */
std::string sessionJson(const Session& session, int resolvedPort, const std::string& resolvedActor)
{
	std::ostringstream out;
	out << "{\"path\":" << jsonString(session.path)
	    << ",\"exists\":" << jsonBool(session.exists)
	    << ",\"port\":" << (session.hasPort ? jsonNumber(session.port) : std::string("null"))
	    << ",\"actor\":" << (session.hasActor ? jsonString(session.actor) : std::string("null"))
	    << ",\"resolved\":{\"port\":" << resolvedPort
	    << ",\"actor\":" << jsonString(resolvedActor) << "}}";
	return out.str();
}

std::string sessionHuman(const Session& session, int resolvedPort, const std::string& resolvedActor)
{
	std::ostringstream out;
	out << "session: " << session.path << (session.exists ? "" : " (absent)") << "\n";
	out << "port: ";
	if (session.hasPort) out << session.port; else out << "-";
	out << "\nactor: ";
	if (session.hasActor) out << session.actor; else out << "-";
	out << "\nresolved: port=" << resolvedPort << " actor=" << resolvedActor;
	return out.str();
}

} // namespace

CommandResult configCommand(const std::vector<std::string>& tokens)
{
	Args args(tokens);
	std::string error;
	if (!args.validate(std::vector<std::string>(),
	                   std::vector<std::string>{ "--port", "--actor" }, &error))
	{
		return CommandResult::fail(KUsage, error);
	}

	const std::vector<std::string> positional = args.positional();
	if (positional.empty())
	{
		return CommandResult::fail(KUsage, "config requires a subcommand: show | set | clear");
	}
	const std::string sub = positional[0];

	std::string warning;
	Session session = loadSession(&warning);

	if ("show" == sub)
	{
		int port = session.hasPort ? session.port : kDefaultPort;
		std::string actor = session.hasActor ? session.actor : kDefaultActor;
		if (!resolvePortAndActor(args, &port, &actor, &error))
		{
			return CommandResult::fail(KUsage, error);
		}
		CommandResult result = CommandResult::succeed(
			sessionJson(session, port, actor), sessionHuman(session, port, actor));
		result.message = warning;
		return result;
	}

	if ("set" == sub)
	{
		const bool hasPort = args.has("--port");
		const bool hasActor = args.has("--actor");
		if (!hasPort && !hasActor)
		{
			return CommandResult::fail(KUsage, "config set requires --port and/or --actor");
		}

		int port = session.hasPort ? session.port : kDefaultPort;
		std::string actor = session.hasActor ? session.actor : kDefaultActor;
		if (!resolvePortAndActor(args, &port, &actor, &error))
		{
			return CommandResult::fail(KUsage, error);
		}

		if (!saveSession(hasPort || session.hasPort, port,
		                 hasActor || session.hasActor, actor, &error))
		{
			return CommandResult::fail(KIoError, error);
		}

		session = loadSession(NULL);
		std::ostringstream human;
		human << "session updated: " << session.path << "\n"
		      << "port: " << port << "\nactor: " << actor;
		return CommandResult::succeed(sessionJson(session, port, actor), human.str());
	}

	if ("clear" == sub)
	{
		if (!clearSession(&error))
		{
			return CommandResult::fail(KIoError, error);
		}
		session = loadSession(NULL);
		CommandResult result = CommandResult::succeed(
			sessionJson(session, kDefaultPort, kDefaultActor),
			"session cleared: " + session.path);
		return result;
	}

	return CommandResult::fail(KUsage, "unknown config subcommand: " + sub + " (use show | set | clear)");
}

} // namespace mgcli
