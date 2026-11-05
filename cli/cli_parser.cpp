/**
 * @file  cli_parser.cpp
 * @brief CLI 公共设施实现：参数解析、JSON 序列化、REPL 行切分、stdout 分流。
 * @ingroup mgsdk_cli
 */
#include "cli_parser.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#endif

namespace mgcli {

namespace {

bool startsWith(const std::string& text, const std::string& prefix)
{
	return text.size() >= prefix.size() && 0 == text.compare(0, prefix.size(), prefix);
}

bool contains(const std::vector<std::string>& list, const std::string& value)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i] == value)
		{
			return true;
		}
	}
	return false;
}

/** @brief 选项名（去掉 --key=value 的 =value 部分）。 */
std::string optionName(const std::string& token)
{
	const std::string::size_type eq = token.find('=');
	return (std::string::npos == eq) ? token : token.substr(0, eq);
}

/** @brief 路径是否为已存在的目录（fopen 打不开目录，必须用 stat）。 */
bool pathIsDirectory(const std::string& path)
{
	struct stat info;
	if (stat(path.c_str(), &info) != 0) return false;
	return (info.st_mode & S_IFDIR) != 0;
}

} // namespace

CommandResult::CommandResult() : ok(false), code(KOk), data("{}") {}

CommandResult CommandResult::succeed(const std::string& data, const std::string& human)
{
	CommandResult result;
	result.ok = true;
	result.code = KOk;
	result.data = data.empty() ? std::string("{}") : data;
	result.human = human;
	return result;
}

CommandResult CommandResult::fail(int code, const std::string& message,
                                 const std::string& data, const std::string& human)
{
	CommandResult result;
	result.ok = false;
	result.code = code;
	result.message = message;
	result.data = data.empty() ? std::string("{}") : data;
	result.human = human;
	return result;
}

std::string CommandResult::envelopeJson() const
{
	std::ostringstream out;
	out << "{\"ok\":" << (ok ? "true" : "false")
	    << ",\"code\":" << code
	    << ",\"message\":" << jsonString(message)
	    << ",\"data\":" << (data.empty() ? std::string("{}") : data)
	    << "}";
	return out.str();
}

Args::Args(const std::vector<std::string>& tokens) : tokens_(tokens) {}

bool Args::has(const std::string& name) const
{
	for (size_t i = 0; i < tokens_.size(); ++i)
	{
		if (tokens_[i] == name || optionName(tokens_[i]) == name)
		{
			return startsWith(tokens_[i], name);
		}
	}
	return false;
}

const std::string* Args::findValue(const std::string& name, size_t* inlinePos) const
{
	for (size_t i = 0; i < tokens_.size(); ++i)
	{
		if (tokens_[i] == name)
		{
			if (inlinePos != NULL) *inlinePos = std::string::npos;
			return &tokens_[i];
		}
		if (startsWith(tokens_[i], name) && tokens_[i].size() > name.size()
		    && tokens_[i][name.size()] == '=')
		{
			if (inlinePos != NULL) *inlinePos = name.size() + 1;
			return &tokens_[i];
		}
	}
	return NULL;
}

std::string Args::value(const std::string& name, const std::string& def) const
{
	/* 取最后一次出现，便于 REPL/脚本覆盖前面的默认值 */
	for (size_t i = tokens_.size(); i-- > 0;)
	{
		const std::string& token = tokens_[i];
		if (token == name)
		{
			if (i + 1 < tokens_.size() && !startsWith(tokens_[i + 1], "--"))
			{
				return tokens_[i + 1];
			}
			return def;
		}
		if (startsWith(token, name) && token.size() > name.size() && token[name.size()] == '=')
		{
			return token.substr(name.size() + 1);
		}
	}
	return def;
}

std::vector<std::string> Args::values(const std::string& name) const
{
	std::vector<std::string> result;
	for (size_t i = 0; i < tokens_.size(); ++i)
	{
		if (tokens_[i] == name)
		{
			if (i + 1 < tokens_.size() && !startsWith(tokens_[i + 1], "--"))
			{
				result.push_back(tokens_[i + 1]);
			}
		}
		else if (startsWith(tokens_[i], name) && tokens_[i].size() > name.size()
		         && tokens_[i][name.size()] == '=')
		{
			result.push_back(tokens_[i].substr(name.size() + 1));
		}
	}
	return result;
}

bool Args::validate(const std::vector<std::string>& flags,
                    const std::vector<std::string>& options,
                    std::string* error) const
{
	for (size_t i = 0; i < tokens_.size(); ++i)
	{
		const std::string& token = tokens_[i];
		if (token.empty() || token[0] != '-')
		{
			continue;
		}

		const std::string name = optionName(token);
		const bool hasInline = token.find('=') != std::string::npos;
		const bool isFlag = contains(flags, name);
		const bool isOption = contains(options, name);

		if (!isFlag && !isOption)
		{
			if (error != NULL) *error = "unknown option: " + name;
			return false;
		}
		if (isOption && !hasInline)
		{
			if (i + 1 >= tokens_.size() || startsWith(tokens_[i + 1], "--"))
			{
				if (error != NULL) *error = "missing value for " + name;
				return false;
			}
			++i;
		}
	}
	return true;
}

bool Args::getInt(const std::string& name, int def, int* out, std::string* error) const
{
	if (!has(name))
	{
		*out = def;
		return true;
	}
	const std::string text = value(name);
	if (text.empty())
	{
		if (error != NULL) *error = "missing value for " + name;
		return false;
	}
	char* end = NULL;
	const long parsed = strtol(text.c_str(), &end, 10);
	if (end == NULL || *end != 0)
	{
		if (error != NULL) *error = "invalid integer for " + name + ": " + text;
		return false;
	}
	*out = static_cast<int>(parsed);
	return true;
}

std::vector<std::string> Args::positional() const
{
	std::vector<std::string> result;
	for (size_t i = 0; i < tokens_.size(); ++i)
	{
		if (!tokens_[i].empty() && tokens_[i][0] == '-')
		{
			if (tokens_[i].find('=') == std::string::npos)
			{
				++i; /* 跳过选项取值 */
			}
			continue;
		}
		result.push_back(tokens_[i]);
	}
	return result;
}

std::vector<std::string> splitCsv(const std::string& text)
{
	std::vector<std::string> result;
	std::string current;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == ',')
		{
			const std::string item = trim(current);
			if (!item.empty()) result.push_back(item);
			current.clear();
		}
		else
		{
			current += text[i];
		}
	}
	const std::string item = trim(current);
	if (!item.empty()) result.push_back(item);
	return result;
}

std::string trim(const std::string& text)
{
	size_t begin = 0;
	size_t end = text.size();
	while (begin < end)
	{
		const char ch = text[begin];
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ++begin;
		else break;
	}
	while (end > begin)
	{
		const char ch = text[end - 1];
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') --end;
		else break;
	}
	return text.substr(begin, end - begin);
}

bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		char ca = a[i];
		char cb = b[i];
		if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
		if (ca != cb) return false;
	}
	return true;
}

bool isAllDigits(const std::string& text)
{
	if (text.empty()) return false;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] < '0' || text[i] > '9') return false;
	}
	return true;
}

std::string jsonEscape(const std::string& text)
{
	std::string out;
	out.reserve(text.size() + 8);
	for (size_t i = 0; i < text.size(); ++i)
	{
		const unsigned char ch = static_cast<unsigned char>(text[i]);
		switch (ch)
		{
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (ch < 0x20)
			{
				char buffer[8] = { 0 };
				snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
				out += buffer;
			}
			else
			{
				out += static_cast<char>(ch);
			}
			break;
		}
	}
	return out;
}

std::string jsonString(const std::string& text)
{
	return "\"" + jsonEscape(text) + "\"";
}

std::string jsonNumber(double value)
{
	if (!std::isfinite(value))
	{
		return "null";
	}
	char buffer[64] = { 0 };
	snprintf(buffer, sizeof(buffer), "%.7g", value);
	return buffer;
}

std::string jsonBool(bool value)
{
	return value ? "true" : "false";
}

std::string formatFixed(double value, int precision)
{
	if (!std::isfinite(value))
	{
		return "n/a";
	}
	char buffer[64] = { 0 };
	snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
	std::string text(buffer);
	if (text.find('.') != std::string::npos)
	{
		while (!text.empty() && text[text.size() - 1] == '0') text.erase(text.size() - 1);
		if (!text.empty() && text[text.size() - 1] == '.') text.erase(text.size() - 1);
	}
	if (text == "-0") text = "0";
	return text;
}

std::string localTimeIso8601()
{
	std::time_t now = std::time(NULL);
	std::tm local;
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char buffer[32] = { 0 };
	strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local);
	return buffer;
}

std::string executablePath()
{
#ifdef _WIN32
	char buffer[MAX_PATH] = { 0 };
	const DWORD length = GetModuleFileNameA(NULL, buffer, static_cast<DWORD>(sizeof(buffer)));
	if (length == 0 || length >= sizeof(buffer))
	{
		return std::string();
	}
	return std::string(buffer, length);
#else
	char buffer[PATH_MAX] = { 0 };
	const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (length <= 0)
	{
		return std::string();
	}
	return std::string(buffer, static_cast<size_t>(length));
#endif
}

std::string tempDirectory()
{
	const char* value = getenv("TMPDIR");
	if (value == NULL || value[0] == 0) value = getenv("TEMP");
	if (value == NULL || value[0] == 0) value = getenv("TMP");
	if (value == NULL || value[0] == 0) return std::string(".");
	return std::string(value);
}

bool fileExists(const std::string& path)
{
	FILE* file = fopen(path.c_str(), "rb");
	if (file == NULL) return false;
	fclose(file);
	return true;
}

bool readWholeFile(const std::string& path, std::string* content, std::string* error)
{
	FILE* file = fopen(path.c_str(), "rb");
	if (file == NULL)
	{
		if (error != NULL) *error = "cannot open file for reading: " + path;
		return false;
	}
	std::string buffer;
	char chunk[4096];
	size_t readBytes = 0;
	while ((readBytes = fread(chunk, 1, sizeof(chunk), file)) > 0)
	{
		buffer.append(chunk, readBytes);
	}
	fclose(file);
	*content = buffer;
	return true;
}

bool writeWholeFile(const std::string& path, const std::string& content, std::string* error)
{
	FILE* file = fopen(path.c_str(), "wb");
	if (file == NULL)
	{
		if (error != NULL) *error = "cannot open file for writing: " + path;
		return false;
	}
	const size_t written = fwrite(content.data(), 1, content.size(), file);
	const bool flushed = (0 == fflush(file));
	fclose(file);
	if (written != content.size() || !flushed)
	{
		if (error != NULL) *error = "failed to write file: " + path;
		return false;
	}
	return true;
}

bool ensureDirectory(const std::string& path, std::string* error)
{
	if (path.empty())
	{
		if (error != NULL) *error = "empty directory path";
		return false;
	}

	std::string normalized = path;
	for (size_t i = 0; i < normalized.size(); ++i)
	{
		if (normalized[i] == '\\') normalized[i] = '/';
	}
	if (!normalized.empty() && normalized[normalized.size() - 1] == '/')
	{
		normalized.erase(normalized.size() - 1);
	}

	std::string current;
	size_t index = 0;
	/* 保留 Windows 盘符（C:）或 POSIX 根（/）前缀 */
	if (normalized.size() >= 2 && normalized[1] == ':')
	{
		current = normalized.substr(0, 2);
		index = 2;
	}
	else if (!normalized.empty() && normalized[0] == '/')
	{
		current = "/";
		index = 1;
	}

	while (index <= normalized.size())
	{
		const std::string::size_type slash = normalized.find('/', index);
		const std::string segment = normalized.substr(
			index, (slash == std::string::npos) ? std::string::npos : slash - index);
		if (!segment.empty())
		{
			if (!current.empty() && current[current.size() - 1] != '/') current += '/';
			current += segment;
			if (!pathIsDirectory(current))
			{
#ifdef _WIN32
				const int ret = _mkdir(current.c_str());
#else
				const int ret = mkdir(current.c_str(), 0755);
#endif
				if (ret != 0 && !pathIsDirectory(current))
				{
					if (error != NULL) *error = "cannot create directory: " + current;
					return false;
				}
			}
		}
		if (slash == std::string::npos) break;
		index = slash + 1;
	}
	return true;
}

std::vector<std::string> splitCommandLine(const std::string& line)
{
	std::string text = line;
	/* 管道/脚本输入常带 UTF-8 BOM（重定向时可能叠加多个），不去掉会导致首条命令识别失败 */
	while (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF
	       && static_cast<unsigned char>(text[1]) == 0xBB
	       && static_cast<unsigned char>(text[2]) == 0xBF)
	{
		text.erase(0, 3);
	}

	std::vector<std::string> tokens;
	std::string current;
	bool inToken = false;
	char quote = 0;

	for (size_t i = 0; i < text.size(); ++i)
	{
		const char ch = text[i];
		if (quote != 0)
		{
			if (ch == quote)
			{
				quote = 0;
			}
			else
			{
				current += ch;
			}
			continue;
		}
		if (ch == '"' || ch == '\'')
		{
			quote = ch;
			inToken = true;
			continue;
		}
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		{
			if (inToken)
			{
				tokens.push_back(current);
				current.clear();
				inToken = false;
			}
			continue;
		}
		current += ch;
		inToken = true;
	}
	if (inToken)
	{
		tokens.push_back(current);
	}
	return tokens;
}

std::string usageText()
{
	return
		"用法:\n"
		"  motionGloveSDK_cli [--json] <命令> [选项]\n"
		"  motionGloveSDK_cli                      无参数进入 REPL\n"
		"\n"
		"命令:\n"
		"  status                        SDK 版本、默认参数与会话状态\n"
		"  config show|set|clear         查看/设置/清除会话默认值（端口、数据流名）\n"
		"  run [选项]                    监听 UDP 并采集手套帧（别名 listen / capture）\n"
		"  export --output <文件> [选项] 采集并导出 JSON / CSV\n"
		"  version                       版本信息\n"
		"  help [命令]                   帮助\n"
		"  repl                          进入交互式 REPL\n"
		"\n"
		"通用选项:\n"
		"  --json                        输出统一 JSON 信封\n"
		"  --port <1024-65535>           监听端口（默认 5001，可被会话覆盖）\n"
		"  --actor <名称>                数据流名称前缀（默认 Glove1，最长 63 字节）\n"
		"\n"
		"run / export 选项:\n"
		"  --frames <n>                  采集帧数，0 表示直到超时（默认 1）\n"
		"  --timeout-ms <n>              超时毫秒（默认 3000）\n"
		"  --poll-ms <n>                 轮询间隔毫秒（默认 10）\n"
		"  --joint <名称|序号>           只输出指定骨骼，可重复\n"
		"  --fields pos,euler,quat       输出字段（默认全部）\n"
		"\n"
		"export 额外选项:\n"
		"  --output <路径>               输出文件（.json / .csv）\n"
		"  --format json|csv             显式指定格式（默认按扩展名推断）\n"
		"\n"
		"REPL:\n"
		"  直接输入命令；支持 exit / quit、help、json on|off\n"
		"\n"
		"退出码: 0 成功；1 参数错误；2 端口/SDK 错误；3 超时未收到帧；4 文件读写错误；5 取帧失败\n";
}

StdoutRedirect::StdoutRedirect(bool enable) : active_(false), saved_(-1)
{
	if (!enable)
	{
		return;
	}
	fflush(stdout);
#ifdef _WIN32
	saved_ = _dup(1);
	if (saved_ >= 0 && _dup2(2, 1) == 0)
	{
		active_ = true;
	}
#else
	saved_ = dup(STDOUT_FILENO);
	if (saved_ >= 0 && dup2(STDERR_FILENO, STDOUT_FILENO) >= 0)
	{
		active_ = true;
	}
#endif
}

StdoutRedirect::~StdoutRedirect()
{
	if (!active_)
	{
		return;
	}
	fflush(stdout);
#ifdef _WIN32
	_dup2(saved_, 1);
	_close(saved_);
#else
	dup2(saved_, STDOUT_FILENO);
	close(saved_);
#endif
}

} // namespace mgcli
