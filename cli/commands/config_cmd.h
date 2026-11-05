/**
 * @file  config_cmd.h
 * @brief `config` 命令：查看/设置/清除会话默认值（端口、数据流名）。
 * @ingroup mgsdk_cli
 */
#pragma once

#include <string>
#include <vector>

#include "cli_parser.h"

namespace mgcli {

/** @brief `config show|set|clear` 命令入口。 */
CommandResult configCommand(const std::vector<std::string>& tokens);

} // namespace mgcli
