/**
 * @file  status_cmd.h
 * @brief `status` / `version` 命令：自检与版本信息，只读、无副作用。
 * @ingroup mgsdk_cli
 */
#pragma once

#include <string>
#include <vector>

#include "cli_parser.h"

namespace mgcli {

/** @brief `status` 命令入口。 */
CommandResult statusCommand(const std::vector<std::string>& tokens);

/** @brief `version` 命令入口。 */
CommandResult versionCommand(const std::vector<std::string>& tokens);

} // namespace mgcli
