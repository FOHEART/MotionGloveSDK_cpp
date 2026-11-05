/**
 * @file  cli_parser.h
 * @brief CLI 公共设施：统一 JSON 信封、参数解析、JSON 序列化、REPL 行切分、stdout 分流。
 * @ingroup mgsdk_cli
 *
 * @details
 * 模块边界：纯工具层，不依赖 SDK 业务接口；被 main.cpp 与 commands/ 共用。
 * 约定：
 *  - 所有命令都返回 #mgcli::CommandResult，其中 data 是 JSON 对象文本，
 *    human 是人类可读文本；是否输出 JSON 由调用方（--json）决定；
 *  - JSON 由手写序列化产生，字段顺序稳定，便于人工比对与测试；
 *  - 非有限浮点（NaN/Inf）序列化为 null，保证输出始终是合法 JSON。
 */
#pragma once

#include <string>
#include <vector>

namespace mgcli {

/** @brief 统一错误码：0 成功，1 参数/用法错误，其余映射到底层领域错误。 */
enum ErrorCode {
	KOk = 0,        ///< 成功
	KUsage = 1,     ///< 参数/用法错误（未知命令、缺必填项、取值越界）
	KSdkError = 2,  ///< SDK/网络错误（UDP 绑定失败、端口被占用）
	KTimeout = 3,   ///< 超时未收到任何帧
	KIoError = 4,   ///< 文件读写错误（导出目录不存在、写入失败）
	KFrameError = 5 ///< 取帧失败（格式未知、缓存帧不可读）
};

/**
 * @brief 单条命令的执行结果：机器可读信封 + 人类可读文本。
 */
struct CommandResult {
	bool ok;             ///< 是否成功
	int code;            ///< 错误码，见 #mgcli::ErrorCode
	std::string message; ///< 失败原因或成功附注（可为空）
	std::string data;    ///< JSON 对象文本，成功/失败都必须存在，至少 "{}"
	std::string human;   ///< 人类可读输出（不含结尾换行），可为空

	CommandResult();
	/** @brief 构造成功结果。 */
	static CommandResult succeed(const std::string& data, const std::string& human = "");
	/** @brief 构造失败结果。 */
	static CommandResult fail(int code, const std::string& message,
	                          const std::string& data = std::string("{}"),
	                          const std::string& human = std::string());
	/** @brief 序列化为单行 JSON 信封 {"ok":..,"code":..,"message":..,"data":..}。 */
	std::string envelopeJson() const;
};

/**
 * @brief 命令行参数视图：只做取值与校验，不打印、不退出进程。
 * @details 支持 `--key value` 与 `--key=value` 两种写法；重复选项按出现顺序返回。
 *          校验失败通过 error 回传（而非抛异常/退出），以便 REPL 内复用。
 */
class Args {
public:
	/** @brief 用命令名之后的参数列表构造。 */
	explicit Args(const std::vector<std::string>& tokens);

	/** @brief 原始参数列表。 */
	const std::vector<std::string>& tokens() const { return tokens_; }

	/** @brief 是否出现某布尔开关（如 "--verbose"）。 */
	bool has(const std::string& name) const;

	/** @brief 取某选项的最后一个值，未出现时返回 def。 */
	std::string value(const std::string& name, const std::string& def = std::string()) const;

	/** @brief 取某可重复选项的全部值。 */
	std::vector<std::string> values(const std::string& name) const;

	/**
	 * @brief 校验选项集合：未知选项、缺少取值都报错。
	 * @param[in]  flags   允许的布尔开关（如 "--verbose"）。
	 * @param[in]  options 允许的带值选项（如 "--port"）。
	 * @param[out] error   失败原因。
	 * @return true 全部合法；false 时 error 已填充。
	 */
	bool validate(const std::vector<std::string>& flags,
	              const std::vector<std::string>& options,
	              std::string* error) const;

	/**
	 * @brief 读取整数选项。
	 * @param[in]  name  选项名，如 "--port"。
	 * @param[in]  def   默认值。
	 * @param[out] out   解析结果。
	 * @param[out] error 失败原因（非整数/越界）。
	 * @return true 成功；false 时 error 已填充。
	 */
	bool getInt(const std::string& name, int def, int* out, std::string* error) const;

	/** @brief 不属于任何选项值的裸参数。 */
	std::vector<std::string> positional() const;

private:
	/** @brief 查找选项第一次出现的位置；name 可带或不带 "--"。 */
	const std::string* findValue(const std::string& name, size_t* inlinePos) const;

	std::vector<std::string> tokens_;
};

/** @brief 按逗号切分（用于 --fields pos,euler / --joint a,b），自动去空白与空项。 */
std::vector<std::string> splitCsv(const std::string& text);

/** @brief 去首尾空白（空格、\t、\r、\n）。 */
std::string trim(const std::string& text);

/** @brief 大小写不敏感比较（仅 ASCII）。 */
bool equalsIgnoreCase(const std::string& a, const std::string& b);

/** @brief 是否全部为十进制数字。 */
bool isAllDigits(const std::string& text);

/** @brief JSON 字符串转义（含引号），返回不含外层引号的文本。 */
std::string jsonEscape(const std::string& text);

/** @brief JSON 字符串字面量（带双引号）。 */
std::string jsonString(const std::string& text);

/** @brief JSON 数值：非有限值输出 null，其它按最短安全形式输出。 */
std::string jsonNumber(double value);

/** @brief JSON 布尔字面量。 */
std::string jsonBool(bool value);

/** @brief 固定精度数值转文本并去掉多余的 0（用于人类可读输出）。 */
std::string formatFixed(double value, int precision);

/** @brief 当前本地时间，格式 YYYY-MM-DDTHH:MM:SS。 */
std::string localTimeIso8601();

/** @brief 进程可执行文件的绝对路径（取不到时返回空串）。 */
std::string executablePath();

/** @brief 系统临时目录（取不到时返回 "."）。 */
std::string tempDirectory();

/** @brief 文件是否存在且可读。 */
bool fileExists(const std::string& path);

/** @brief 逐字节读文件；失败时填充 error。 */
bool readWholeFile(const std::string& path, std::string* content, std::string* error);

/** @brief 写文件（覆盖）；失败时填充 error。 */
bool writeWholeFile(const std::string& path, const std::string& content, std::string* error);

/** @brief 确保目录存在（逐级创建）；失败时填充 error。 */
bool ensureDirectory(const std::string& path, std::string* error);

/**
 * @brief REPL 行切分：剥离 UTF-8 BOM、按空白切分、支持成对单/双引号。
 * @param[in] line 一行输入（可含 BOM 与 CR）。
 * @return 参数列表；空行返回空列表。
 */
std::vector<std::string> splitCommandLine(const std::string& line);

/** @brief 帮助文本（REPL 与 help 命令共用）。 */
std::string usageText();

/**
 * @brief 临时把 stdout 重定向到 stderr。
 * @details SDK 的接收线程会直接 printf 诊断日志；JSON 模式下若与信封混在
 *          同一路 stdout，任何解析器都会被污染。构造时保存 fd 1 并 dup2(2,1)，
 *          析构时还原；进程内的线程共享 fd 表，因此接收线程的日志也会改道。
 *          已关闭（enable=false）时为空操作。
 */
class StdoutRedirect {
public:
	explicit StdoutRedirect(bool enable);
	~StdoutRedirect();

private:
	bool active_;
	int saved_;
	StdoutRedirect(const StdoutRedirect&);
	StdoutRedirect& operator=(const StdoutRedirect&);
};

} // namespace mgcli
