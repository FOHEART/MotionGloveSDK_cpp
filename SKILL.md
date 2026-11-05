---
name: cli-anything-motionglovesdk
description: 控制 MotionGlove 手套 SDK 的命令行接口：查看版本/会话、监听 UDP 采集 32/42 段骨骼帧、导出 JSON/CSV。全部命令支持 --json 统一信封。
---

# MotionGloveSDK CLI Skill

`motionGloveSDK_cli` 是 MotionGloveSDK 的无头命令行（控制台程序，不依赖 GUI/窗口），
用于在脚本、CI 或 Agent 中：

- 确认 SDK 版本、当前会话默认值与二进制位置；
- 监听 UDP 端口，按需采集 N 帧手套数据（32 段 / 42 段自动识别）；
- 把采集结果导出为 JSON 或 CSV；
- 在无硬件环境下用仓库自带的 mock 发送端做闭环验证。

数据来源是 MotionVenus / MotionGlove 软件的"数据转发"（CSV 字符流，默认 UDP `5001`）。

## 两种调用方式

1. **直调可执行文件（推荐用于 shell / CI）**

   ```console
   motionGloveSDK_cli --json status
   motionGloveSDK_cli run --frames 2 --timeout-ms 3000
   motionGloveSDK_cli export --frames 60 --timeout-ms 5000 --output out/cap.csv
   ```

2. **Python harness（推荐用于 Agent / 单测）**

   ```console
   pip install -e agent-harness
   motionglovesdk-cli --cli-bin path/to/motionGloveSDK_cli run --frames 2
   ```

   harness 会按 `MOTIONGLOVESDK_CLI_BIN` > 部署目录 > 仓库 `out/` 构建产物 > `PATH`
   的顺序自动定位 exe，也可以在代码里直接调用：

   ```python
   from cli_anything.motionglovesdk.utils import backend
   result = backend.run_cli(backend.build_argv("status"))
   print(result.ok, result.data["sdkVersion"])
   ```

## 部署目录布局

`scripts\deploy.bat` 会把 CLI 与 harness 一起汇总到可运行目录（harness 靠相对位置
自动找到同级 exe，用户无需配置）：

```
out/install_example/x64-<配置>/
├── motionGloveSDK_cli.exe      # CLI 本体
├── MotionGloveSDK.dll          # SDK 运行库
├── SKILL.md                    # 本文档（唯一来源，不要在 harness 里再放副本）
├── agent-harness/              # Python harness
│   ├── setup.py
│   └── cli_anything/motionglovesdk/...
└── motionGloveSDK_example*.exe # 其它示例
```

## 环境变量

| 变量 | 作用 |
| --- | --- |
| `MOTIONGLOVESDK_CLI_BIN` | harness 使用的 CLI 路径（显式指定优先于所有自动定位） |
| `MOTIONGLOVESDK_CLI_SESSION` | 会话文件路径覆盖（默认 `%TEMP%\motionglove-cli\session.json`），测试隔离用 |
| `MOTIONGLOVE_SDK_UDP_DEBUG` | 设为 `1` 时 SDK 打印收包诊断（输出在 stderr） |

## 统一 JSON 信封

所有命令都支持 `--json`（可放在任意位置），stdout 只有一行信封：

```json
{"ok":true,"code":0,"message":"","data":{}}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ok` | bool | 业务是否成功（与进程退出码一致：成功 0 / 失败 1） |
| `code` | int | 细分错误码，见下表；成功时为 0 |
| `message` | string | 失败原因或成功附注（例如只收到部分帧时的超时说明） |
| `data` | object | 各命令的数据，永远存在（失败时可能为 `{}`） |

> SDK 接收线程的诊断日志会被 CLI 改道到 **stderr**，stdout 始终只有信封。

### `data` 字段 schema

`status`：

```json
{"cliVersion":"0.2.1","sdkVersion":"0.0.9","sdkVersionRaw":9,
 "exe":"D:\\...\\motionGloveSDK_cli.exe","platform":"windows","transport":"udp-csv",
 "defaults":{"port":5001,"actor":"Glove1","timeoutMs":3000},
 "session":{"path":"C:\\...\\session.json","exists":false,"port":null,"actor":null},
 "resolved":{"port":5001,"actor":"Glove1"}}
```

`config show|set|clear`：

```json
{"path":"C:\\...\\session.json","exists":true,"port":5002,"actor":"Glove1",
 "resolved":{"port":5002,"actor":"Glove1"}}
```

`run`：

```json
{"port":15021,"actor":"Glove1","requestedFrames":2,"frameCount":2,"complete":true,
 "elapsedMs":36,"timeoutMs":4000,"pollMs":10,"listenRet":0,"frameErrors":0,"layout":42,
 "frames":[{"frameNumber":37,"layout":42,"skeletonCount":42,
            "protocolVersion":0,"suitNumber":0,"suitType":"FOHEART_X",
            "streamFormat":"binary","position":"meter","attitude":"quat",
            "coordinate":"relative","channelOrder":"XYZ",
            "leftHandGesture":0,"rightHandGesture":0,
            "remoteIp":"127.0.0.1","remotePort":64463,
            "skeletons":[{"index":21,"name":"LeftHand",
                          "pos":[0.046254,0.127655,-0.031013],
                          "quat":[0,-0.000169,0,1]}]}]}
```

- `layout` 是骨骼段数（`32` / `42`），不是枚举值；无帧时为 `null`。
- `pos` 单位米，`euler` 单位度，`quat` 顺序 `xyzw`。
- `--fields` 可裁剪 `pos`/`euler`/`quat`；`--joint` 可裁剪 `skeletons` 数组。

`export`：

```json
{"output":"out/cap.csv","format":"csv","bytes":11158,"port":5001,
 "actor":"Glove1","frameCount":3,"layout":42,"elapsedMs":55}
```

导出文件内容：

- JSON：`{exportedAt, cliVersion, sdkVersion, port, actor, frameCount, layout, frames[]}`；
- CSV 表头：`frame_number,skeleton_count,skeleton_index,skeleton_name,pos_x,pos_y,pos_z,euler_x,euler_y,euler_z,quat_x,quat_y,quat_z,quat_w`
  （`--fields` 会裁剪对应列；每个骨骼一行）。

## 命令

### `status`

查看版本、平台、默认参数与会话状态；只读，不绑定端口。

| 参数 | 必填 | 说明 |
| --- | --- | --- |
| `--port` | 否 | 预览解析后的端口（不写入会话） |
| `--actor` | 否 | 预览解析后的数据流名 |
| `--json` | 否 | 输出信封 |

```console
motionGloveSDK_cli --json status
```

### `config show|set|clear`

会话默认值跨进程保存在固定路径的 JSON 文件里；**优先级：命令行 > 会话 > 内置默认**。

| 子命令 | 参数 | 说明 |
| --- | --- | --- |
| `show` | `--port`/`--actor`（可选预览） | 打印会话与解析结果 |
| `set` | `--port` 和/或 `--actor` | 至少给一项，其余字段保留 |
| `clear` | 无 | 删除会话文件，恢复 `5001` / `Glove1` |

```console
motionGloveSDK_cli config set --port 5002 --actor Glove1
motionGloveSDK_cli --json config show
motionGloveSDK_cli config clear
```

### `run`（别名 `listen` / `capture`）

绑定 `0.0.0.0:<port>`，轮询新帧直到收满 `--frames` 或超时，然后关闭端口。

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--port` | 5001（或会话值） | UDP 监听端口，范围 `[1024, 65535]` |
| `--actor` | Glove1（或会话值） | 数据流名**前缀**，1..63 字节 |
| `--frames` | 1 | 采集帧数；`0` 表示一直采到超时 |
| `--timeout-ms` | 3000 | 总超时（毫秒），从绑定成功后开始计时 |
| `--poll-ms` | 10 | 轮询间隔（毫秒），范围 `[1, 1000]` |
| `--joint` | 全部 | 只输出指定骨骼，可重复；取值为 42/32 段骨骼名或下标 |
| `--fields` | `pos,euler,quat` | 输出字段子集 |

```console
motionGloveSDK_cli --json run --frames 2 --timeout-ms 3000 --joint LeftHand --fields pos,quat
motionGloveSDK_cli run --frames 0 --timeout-ms 1000      # 采 1 秒内的全部帧（人类可读）
```

### `export`

与 `run` 相同的采集参数，另加：

| 参数 | 必填 | 说明 |
| --- | --- | --- |
| `--output` | 是 | 输出文件；目录必须已存在（不会自动创建） |
| `--format` | 否 | `json` / `csv`；缺省按扩展名推断，扩展名不可推断时报参数错误 |

```console
motionGloveSDK_cli --json export --frames 3 --timeout-ms 5000 --output out/build/cap.csv
motionGloveSDK_cli --json export --frames 2 --timeout-ms 5000 --output cap.dat --format json
```

### `version` / `help` / `repl`

- `version`（或 `--version`）：CLI 与 SDK 版本；
- `help`：完整用法；
- 无参数启动进入 REPL：`exit`/`quit` 退出，`help` 帮助，`json on|off` 切换输出，
  支持管道输入（自动剥离 UTF-8 BOM）与引号路径。

## 错误码对照表

进程退出码只区分成败（0 成功 / 1 失败），细分码在信封 `code` 中：

| code | 含义 | 实测 message 样例 |
| --- | --- | --- |
| 0 | 成功 | `""` |
| 1 | 参数/用法错误 | `unknown command: nosuch; run \`motionGloveSDK_cli help\` for usage`<br>`--port must be in [1024, 65535], got 80`<br>`unknown joint: Foo`<br>`cannot infer export format from 'cap.txt'; add --format json\|csv`<br>`config requires a subcommand: show \| set \| clear` |
| 2 | 端口/SDK 错误 | `bind UDP port 5001 failed (ret=-1); port may be in use (netstat -ano -p UDP \| findstr :5001) or blocked by firewall` |
| 3 | 超时未收到任何帧 | `no frames received within 300 ms on UDP 15002 (actor 'Glove1'); check MotionVenus data forwarding and firewall` |
| 4 | 文件读写错误 | `output directory does not exist: out/no/such`<br>`cannot open file for writing: <path>` |
| 5 | 取帧失败 | `no readable frame from stream 'Glove1' after 3 attempt(s): GetSkeletons42Frame failed (ret=-1)` |
| -1 | （仅 harness）没拿到 CLI 信封 | `motionGloveSDK_cli not found; set MOTIONGLOVESDK_CLI_BIN ...` |

Agent 判定建议：先看信封 `ok`；`code==1` 应改命令重试，`code==2` 检查端口占用/防火墙，
`code==3` 检查 MotionVenus 数据转发与网络，`code==4` 检查输出目录，`code==5` 降低帧率或排查发送端。

## 示例（均已实测）

1. 查看状态：

   ```console
   > motionGloveSDK_cli --json status
   {"ok":true,"code":0,"message":"","data":{"cliVersion":"0.2.1","sdkVersion":"0.0.9",...}}
   ```

2. 采集 2 帧并只保留左手手掌的位置与四元数：

   ```console
   > motionGloveSDK_cli --json run --frames 2 --timeout-ms 4000 --joint LeftHand --fields pos,quat
   {"ok":true,"code":0,"message":"","data":{"port":15021,"frameCount":2,"layout":42,
    "frames":[{"frameNumber":37,"skeletons":[{"index":21,"name":"LeftHand",
    "pos":[0.046254,0.127655,-0.031013],"quat":[0,-0.000169,0,1]}]},...]}}
   ```

   同一命令的人类可读输出（无 `--json`）：

   ```console
   [41][42] LeftHand pos=(0.0482, 0.1249, -0.0366) quat=(0, -0.0063, 0, 1)
   [42][42] LeftHand pos=(0.0485, 0.124, -0.0379) quat=(0, -0.0077, 0, 1)
   frames=2/2 elapsed=44ms rate=45.5Hz
   ```

3. 导出 3 帧到 CSV：

   ```console
   > motionGloveSDK_cli export --frames 3 --timeout-ms 4000 --output out/build/cap.csv
   exported 3 frame(s) to out/build/cap.csv (csv, 14830 bytes)
   ```

4. 会话固定端口后免参数采集：

   ```console
   > motionGloveSDK_cli config set --port 5002
   > motionGloveSDK_cli --json config show
   {"ok":true,...,"data":{"path":"C:\\...\\session.json","exists":true,"port":5002,
     "actor":null,"resolved":{"port":5002,"actor":"Glove1"}}}
   ```

5. 无硬件闭环验证（mock 发送端）：

   ```console
   > start /b python tools\mock_glove_sender.py --port 15001 --seconds 5 --rate 60
   > motionGloveSDK_cli --json run --port 15001 --frames 5 --timeout-ms 4000
   {"ok":true,"code":0,"message":"","data":{"frameCount":5,"layout":42,...}}
   ```

6. REPL（含 BOM 的脚本管道输入）：

   ```console
   > (echo status --json & echo exit) | motionGloveSDK_cli
   motionGloveSDK_cli REPL — 输入 help 查看命令，exit/quit 退出，json on|off 切换输出
   mgsdk> {"ok":true,"code":0,"message":"","data":{...}}
   ```

## 注意事项

- **端口与防火墙**：SDK 绑定 `0.0.0.0`，局域网部署时数据转发目标要填运行 CLI 的机器 IP + 端口，
  并在该机器放行入站 UDP（`tools\add_firewall_rule.bat 5001`）。
- **actor 是前缀匹配**：发送端演员名 `Glove1-SN-XXXX` 用 `--actor Glove1` 即可命中；
  多个演员共存时用更长前缀区分。
- **单位**：位置米、欧拉角度、四元数 `xyzw`；`frameNumber` 由发送端递增，60Hz 数据转发下约 60 帧/秒。
- **一次进程一次采集**：命令无状态；需要"持续流"时把 `--frames 0 --timeout-ms <窗口>` 当作一次采样窗口，
  或在 REPL 内重复执行。
- **导出目录必须存在**：`--output` 的父目录不会自动创建，不存在时返回 code 4。
- **`streamFormat`/`suitType` 等字段来自帧头**：字符串流解析时会保留 SDK 帧头默认值（如 `binary`），
  不要拿它判断真实传输格式。
- **CI/无硬件**：用 `tools/mock_glove_sender.py` 造帧；或只跑 `status` / `version` 等只读命令。
- **harness 测试**：`pytest agent-harness`（找不到 CLI 时 e2e 整组 skip，单测全 mock）。
