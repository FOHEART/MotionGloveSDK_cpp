/**
 * @file  export_cmd.h
 * @brief `export` 命令：采集若干帧并写成 JSON / CSV 文件。
 * @ingroup mgsdk_cli
 */
#pragma once

#include <string>
#include <vector>

#include "cli_parser.h"

namespace mgcli {

/** @brief `export` 命令入口（tokens 为命令名之后的参数）。 */
CommandResult exportCommand(const std::vector<std::string>& tokens);

} // namespace mgcli
