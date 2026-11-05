/**
 * @file  session.h
 * @brief CLI 会话状态：把"默认端口 / 数据流名"落到固定路径的 JSON 文件，跨进程共享。
 * @ingroup mgsdk_cli
 *
 * @details
 * 每条命令都是一次独立进程，默认无状态；只有 `config set` 写入的默认值需要
 * 跨进程保留，因此单独抽一个会话文件：
 *   优先级：命令行显式选项 > 会话文件 > 内置默认值。
 * 路径：环境变量 MOTIONGLOVESDK_CLI_SESSION（便于测试隔离）>
 *       <临时目录>/motionglove-cli/session.json。
 */
#pragma once

#include <string>

namespace mgcli {

/** @brief 会话配置快照。 */
struct Session {
	std::string path;      ///< 会话文件路径
	bool exists;           ///< 文件是否存在且可解析
	bool hasPort;          ///< 是否显式设置了端口
	int port;              ///< 端口值（hasPort 为 true 时有效）
	bool hasActor;         ///< 是否显式设置了数据流名
	std::string actor;     ///< 数据流名（hasActor 为 true 时有效）

	Session();
};

/** @brief 会话文件的固定路径（含环境变量覆盖）。 */
std::string sessionFilePath();

/**
 * @brief 读取会话文件。
 * @param[out] warning 文件存在但内容不可解析时的提示，可为 NULL。
 * @return 会话快照；文件不存在时 exists=false，其余字段为默认值。
 */
Session loadSession(std::string* warning);

/**
 * @brief 写入会话文件（覆盖）。
 * @param[in]  hasPort 是否写入端口。
 * @param[in]  port    端口值。
 * @param[in]  hasActor 是否写入数据流名。
 * @param[in]  actor   数据流名。
 * @param[out] error   失败原因。
 * @return true 成功。
 */
bool saveSession(bool hasPort, int port, bool hasActor, const std::string& actor,
                 std::string* error);

/** @brief 删除会话文件；文件不存在也视为成功。 */
bool clearSession(std::string* error);

} // namespace mgcli
