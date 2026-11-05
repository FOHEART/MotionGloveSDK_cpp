/**
 * @file  main.cpp
 * @brief motionGloveSDK_cli 入口：命令分发 + REPL，全部输出统一 JSON 信封。
 * @ingroup mgsdk_cli
 *
 * @details
 * 形态：独立薄 CLI 可执行文件（控制台子系统），只做参数解析 + 调 SDK + 输出信封，
 * 不含业务逻辑，也不依赖任何窗口/图形环境，可在无头环境与 CI 中运行。
 *
 * 约定：
 *  - `--json` 可出现在任意位置，输出 {"ok":bool,"code":int,"message":str,"data":{}}；
 *  - 进程退出码只区分成败（0 成功 / 1 失败），细分错误码放在信封 code 里
 *    （Windows 的 ExitProcess 是无符号的，负数会被读成大数）；
 *  - JSON 模式下执行命令期间把 stdout 临时改道 stderr，避免 SDK 接收线程的
 *    诊断日志混进信封；
 *  - 无参数启动进入 REPL，支持 exit/quit、help、json on|off，EOF 正常收尾。
 */
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "cli_parser.h"
#include "commands/config_cmd.h"
#include "commands/export_cmd.h"
#include "commands/run_cmd.h"
#include "commands/status_cmd.h"

namespace {

/** @brief 帮助命令的结果（JSON 模式下把用法文本装进 data.usage）。 */
mgcli::CommandResult helpResult()
{
	const std::string usage = mgcli::usageText();
	return mgcli::CommandResult::succeed("{\"usage\":" + mgcli::jsonString(usage) + "}", usage);
}

/** @brief 按命令名分发；未知命令返回参数错误。 */
mgcli::CommandResult executeCommand(const std::vector<std::string>& tokens)
{
	if (tokens.empty())
	{
		return helpResult();
	}

	const std::string& name = tokens[0];
	const std::vector<std::string> rest(tokens.begin() + 1, tokens.end());

	if (name == "run" || name == "listen" || name == "capture") return mgcli::runCommand(rest);
	if (name == "export") return mgcli::exportCommand(rest);
	if (name == "status") return mgcli::statusCommand(rest);
	if (name == "config") return mgcli::configCommand(rest);
	if (name == "version") return mgcli::versionCommand(rest);
	if (name == "help" || name == "--help" || name == "-h") return helpResult();

	return mgcli::CommandResult::fail(
		mgcli::KUsage,
		"unknown command: " + name + "; run `motionGloveSDK_cli help` for usage");
}

/** @brief 输出命令结果：JSON 信封或人类可读文本。 */
void printResult(const mgcli::CommandResult& result, bool json)
{
	if (json)
	{
		printf("%s\n", result.envelopeJson().c_str());
		fflush(stdout);
		return;
	}

	if (!result.human.empty())
	{
		printf("%s\n", result.human.c_str());
	}
	if (!result.ok)
	{
		/* 失败原因走 stderr，stdout 只留数据/摘要，便于 `2>/dev/null` 只看结果 */
		fprintf(stderr, "[error] code=%d %s\n", result.code, result.message.c_str());
	}
	else if (!result.message.empty())
	{
		printf("note: %s\n", result.message.c_str());
	}
	fflush(stdout);
}

/**
 * @brief 剔除 --json / --no-json，并更新当前输出模式。
 * @param[in,out] tokens 参数列表（原地删除已识别的全局开关）。
 * @param[in,out] json   当前是否为 JSON 模式。
 */
void stripJsonFlags(std::vector<std::string>* tokens, bool* json)
{
	std::vector<std::string> kept;
	for (size_t i = 0; i < tokens->size(); ++i)
	{
		if ((*tokens)[i] == "--json")
		{
			*json = true;
		}
		else if ((*tokens)[i] == "--no-json")
		{
			*json = false;
		}
		else
		{
			kept.push_back((*tokens)[i]);
		}
	}
	tokens->swap(kept);
}

/** @brief 执行一条命令（JSON 模式下自动把 SDK 日志改道 stderr）。 */
mgcli::CommandResult runWithRedirect(const std::vector<std::string>& tokens, bool json)
{
	mgcli::CommandResult result;
	{
		mgcli::StdoutRedirect redirect(json);
		result = executeCommand(tokens);
	}
	return result;
}

/** @brief 交互式 REPL：无参数启动时进入。 */
int runRepl(bool jsonMode)
{
	printf("motionGloveSDK_cli REPL — 输入 help 查看命令，exit/quit 退出，json on|off 切换输出\n");
	fflush(stdout);

	std::string line;
	while (true)
	{
		printf("mgsdk> ");
		fflush(stdout);
		if (!std::getline(std::cin, line))
		{
			printf("\n");
			break;
		}

		std::vector<std::string> tokens = mgcli::splitCommandLine(line);
		bool lineJson = jsonMode;
		stripJsonFlags(&tokens, &lineJson);
		if (tokens.empty())
		{
			if (lineJson != jsonMode)
			{
				jsonMode = lineJson;
				printf("json output: %s\n", jsonMode ? "on" : "off");
			}
			continue;
		}

		const std::string& command = tokens[0];
		if (command == "exit" || command == "quit")
		{
			break;
		}
		if (command == "json")
		{
			if (tokens.size() < 2)
			{
				jsonMode = !jsonMode;
			}
			else
			{
				jsonMode = (tokens[1] == "on" || tokens[1] == "1" || tokens[1] == "true");
			}
			printf("json output: %s\n", jsonMode ? "on" : "off");
			continue;
		}
		if (command == "help" || command == "--help" || command == "-h")
		{
			printf("%s\n", mgcli::usageText().c_str());
			fflush(stdout);
			continue;
		}

		printResult(runWithRedirect(tokens, lineJson), lineJson);
	}
	return 0;
}

} // namespace

int main(int argc, char* argv[])
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif

	std::vector<std::string> tokens;
	for (int i = 1; i < argc; ++i)
	{
		tokens.push_back(argv[i]);
	}

	bool json = false;
	stripJsonFlags(&tokens, &json);

	/* 兼容 `--version` 作为全局开关 */
	if (!tokens.empty() && (tokens[0] == "--version" || tokens[0] == "-v"))
	{
		tokens[0] = "version";
	}

	if (tokens.empty() || tokens[0] == "repl")
	{
		return runRepl(json);
	}

	const mgcli::CommandResult result = runWithRedirect(tokens, json);
	printResult(result, json);
	return result.ok ? 0 : 1;
}
