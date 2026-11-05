/**
 * @file  session.cpp
 * @brief CLI 会话状态实现：极小 JSON 读写，只关心 port / actor 两个字段。
 * @ingroup mgsdk_cli
 */
#include "session.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "cli_parser.h"

namespace mgcli {

namespace {

/** @brief 目录部分（末尾不带分隔符）；没有目录时返回空串。 */
std::string parentDirectory(const std::string& path)
{
	const std::string::size_type slash = path.find_last_of("/\\");
	return (std::string::npos == slash) ? std::string() : path.substr(0, slash);
}

/** @brief 在 JSON 文本中定位 "key" 之后冒号的值起点。 */
const char* findJsonValue(const std::string& text, const std::string& key)
{
	const std::string quoted = "\"" + key + "\"";
	const std::string::size_type keyPos = text.find(quoted);
	if (std::string::npos == keyPos) return NULL;
	const std::string::size_type colon = text.find(':', keyPos + quoted.size());
	if (std::string::npos == colon) return NULL;
	const char* value = text.c_str() + colon + 1;
	while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') ++value;
	return value;
}

bool extractInt(const std::string& text, const std::string& key, int* out)
{
	const char* value = findJsonValue(text, key);
	if (value == NULL) return false;
	char* end = NULL;
	const long parsed = strtol(value, &end, 10);
	if (end == value) return false;
	*out = static_cast<int>(parsed);
	return true;
}

bool extractString(const std::string& text, const std::string& key, std::string* out)
{
	const char* value = findJsonValue(text, key);
	if (value == NULL || *value != '"') return false;
	++value;
	std::string result;
	while (*value != 0 && *value != '"')
	{
		if (*value == '\\' && *(value + 1) != 0)
		{
			++value;
		}
		result += *value;
		++value;
	}
	if (*value != '"') return false;
	*out = result;
	return true;
}

} // namespace

Session::Session()
	: exists(false), hasPort(false), port(0), hasActor(false) {}

std::string sessionFilePath()
{
	const char* overridePath = getenv("MOTIONGLOVESDK_CLI_SESSION");
	if (overridePath != NULL && overridePath[0] != 0)
	{
		return std::string(overridePath);
	}
	return tempDirectory() + "/motionglove-cli/session.json";
}

Session loadSession(std::string* warning)
{
	Session session;
	session.path = sessionFilePath();

	std::string content;
	std::string error;
	if (!readWholeFile(session.path, &content, &error))
	{
		return session;
	}

	int port = 0;
	std::string actor;
	const bool hasPort = extractInt(content, "port", &port);
	const bool hasActor = extractString(content, "actor", &actor);
	if (!hasPort && !hasActor)
	{
		if (warning != NULL) *warning = "session file is not parseable: " + session.path;
		return session;
	}

	session.exists = true;
	session.hasPort = hasPort && port > 0;
	session.port = port;
	session.hasActor = hasActor && !actor.empty();
	session.actor = actor;
	return session;
}

bool saveSession(bool hasPort, int port, bool hasActor, const std::string& actor,
                 std::string* error)
{
	const std::string path = sessionFilePath();
	const std::string directory = parentDirectory(path);
	if (!directory.empty() && !ensureDirectory(directory, error))
	{
		return false;
	}

	std::string content = "{\n";
	if (hasPort)
	{
		content += "  \"port\": " + jsonNumber(static_cast<double>(port)) + ",\n";
	}
	if (hasActor)
	{
		content += "  \"actor\": " + jsonString(actor) + ",\n";
	}
	content += "  \"updatedAt\": " + jsonString(localTimeIso8601()) + "\n}\n";
	return writeWholeFile(path, content, error);
}

bool clearSession(std::string* error)
{
	const std::string path = sessionFilePath();
	if (!fileExists(path))
	{
		return true;
	}
#ifdef _WIN32
	const int ret = _unlink(path.c_str());
#else
	const int ret = unlink(path.c_str());
#endif
	if (ret != 0)
	{
		if (error != NULL) *error = "cannot delete session file: " + path;
		return false;
	}
	return true;
}

} // namespace mgcli
