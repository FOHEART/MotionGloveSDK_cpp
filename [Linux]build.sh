#!/bin/sh
# 兼容入口：构建逻辑统一在 scripts/build.bat（cmd / sh 双解释器），此处只做转发
exec sh "$(dirname "$0")/scripts/build.bat" "$@"
