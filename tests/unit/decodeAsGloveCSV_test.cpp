/**
 * @file  decodeAsGloveCSV_test.cpp
 * @brief 手套 CSV 字符流解析（decodeAsGloveCSV_Func）单元测试，框架为 GoogleTest。
 *
 * 覆盖策略
 *  - 参数防护：空指针（数据流 / 帧头 / 骨骼数组）与错误骨骼数量必须返回 -1；
 *  - 正常路径：单子包整帧解析，帧头字段（名字、姿态、顺序、坐标系、帧号）
 *    与 42 段骨骼数据逐一落位；euler 姿态与 quat 姿态两种格式；
 *  - 分帧协议：多子包拼帧、子包不齐返回 -1、字段个数不符返回 -1、
 *    帧头缺 subpackage 字段返回 -1。
 *
 * 线协议备注：CSV 帧格式为 "帧头,数据段"，帧头以空格分词；
 * euler 姿态每组 6 个字段（pos xyz + euler xyz），quat 姿态每组 7 个字段
 * （pos xyz + quat，线上四元数顺序为 wxyz，SDK 输出转为 xyzw）。
 *
 * 已知约束：SDK 内部使用静态帧管理器（GloveSplitFrameMgr，进程级单例），
 * 各用例必须使用互不相同的帧号，避免跨用例状态串扰（见 TDD 指南 6.2）。
 */
#include "inc/decodeAsGloveCSV.h"

#include <string>
#include <vector>

#include "gtest_assertions.h"

// TEST 宏展开体不参与 Doxygen 文档化（避免 WARN_NO_PARAMDOC 逐个误报）
/// @cond DOC_HIDDEN
namespace {

/// 合法骨骼数量：与协议强一致（42 段手部骨骼）。
constexpr unsigned int kSkeletonCount = static_cast<unsigned int>(KHHS42_SkeletonCount);

/// 每段骨骼在 euler 姿态下的字段数：pos xyz + euler xyz。
constexpr int kEulerFieldsPerSkeleton = 6;

/// 每段骨骼在 quat 姿态下的字段数：pos xyz + quat wxyz。
constexpr int kQuatFieldsPerSkeleton = 7;

/**
 * @brief 把浮点数组拼接为 CSV 片段，每个数值后都跟一个逗号。
 * @details 尾随逗号是多子包拼帧的关键：子包数据直接字符串相加，
 *          若前一子包不以逗号结尾，最后一个数值会与下一子包首数值粘连。
 */
static std::string joinFloatsWithComma(const std::vector<float>& values)
{
    std::string s;
    for (float v : values)
    {
        s += std::to_string(v);
        s += ",";
    }
    return s;
}

/**
 * @brief 构造 CSV 帧头，形如 "Glove1-SN-TEST pos euler ZXY relative fn 100 subpackage 1/1"。
 * @param attitudeToken 姿态标记，"euler" 时附带你旋转顺序 "ZXY"，"quat" 时不带。
 */
static std::string makeHeader(unsigned int frameNumber, int subIndex, int subTotal,
                              const char* attitudeToken)
{
    std::string s = "Glove1-SN-TEST pos ";
    s += attitudeToken;
    if (std::string(attitudeToken) == "euler")
    {
        s += " ZXY";
    }
    s += " relative fn " + std::to_string(frameNumber);
    s += " subpackage " + std::to_string(subIndex) + "/" + std::to_string(subTotal);
    return s;
}

/**
 * @brief 用帧头 + 数据段拼出完整 CSV 帧（帧头与数据段之间以逗号分隔）。
 */
static std::string makeFrame(const std::string& header, const std::vector<float>& values)
{
    return header + "," + joinFloatsWithComma(values);
}

/**
 * @brief 构造带骨骼段数标记的 CSV 帧头。
 * @details 标记位于 `fn <帧号>` 之后、`subpackage` 之前，与线协议一致。
 * @param frameNumber    帧号。
 * @param subIndex       子包序号（从 1 开始）。
 * @param subTotal       子包总数。
 * @param attitudeToken  姿态标记，"euler" 时附带旋转顺序 "ZXY"。
 * @param marker         骨骼段数标记；传空串表示不输出标记（老格式）。
 */
static std::string makeHeaderWithMarker(unsigned int frameNumber, int subIndex, int subTotal,
                                        const char* attitudeToken, const char* marker)
{
    std::string s = "Glove1-SN-TEST pos ";
    s += attitudeToken;
    if (std::string(attitudeToken) == "euler")
    {
        s += " ZXY";
    }
    s += " relative fn " + std::to_string(frameNumber);
    if (marker != nullptr && marker[0] != '\0')
    {
        s += " ";
        s += marker;
    }
    s += " subpackage " + std::to_string(subIndex) + "/" + std::to_string(subTotal);
    return s;
}

/**
 * @brief 构造指定段数的 euler 姿态数据段（32 段 / 42 段用例共用）。
 * @details 第 i 段骨骼的 6 个字段为 (i+0.1, i+0.2, i+0.3, i+0.4, i+0.5, i+0.6)，
 *          各值互不相同以便检出"字段串位"；第 0 段骨骼的欧拉角覆盖为
 *          (0, 0, 90)——这是可手算的解析解输入（绕 Z 轴 +90°）。
 * @param count 骨骼段数；为 0 时返回空向量。
 */
static std::vector<float> makeEulerValues(unsigned int count)
{
    std::vector<float> v;
    v.reserve(count * kEulerFieldsPerSkeleton);
    for (unsigned int i = 0; i < count; ++i)
    {
        v.push_back(static_cast<float>(i) + 0.1f);
        v.push_back(static_cast<float>(i) + 0.2f);
        v.push_back(static_cast<float>(i) + 0.3f);
        v.push_back(static_cast<float>(i) + 0.4f);
        v.push_back(static_cast<float>(i) + 0.5f);
        v.push_back(static_cast<float>(i) + 0.6f);
    }
    if (count > 0)
    {
        v[3] = 0.0f;    // 第 0 段骨骼 eulerX = 0
        v[4] = 0.0f;    // 第 0 段骨骼 eulerY = 0
        v[5] = 90.0f;   // 第 0 段骨骼 eulerZ = 90
    }
    return v;
}

/**
 * @brief 构造 42 段骨骼的 euler 姿态数据段（既有用例沿用此签名）。
 */
static std::vector<float> makeEulerValues()
{
    return makeEulerValues(kSkeletonCount);
}

/**
 * @brief 构造 42 段骨骼的 quat 姿态数据段（每组 7 字段，线上四元数为 wxyz）。
 */
static std::vector<float> makeQuatValues()
{
    std::vector<float> v;
    v.reserve(kSkeletonCount * kQuatFieldsPerSkeleton);
    for (unsigned int i = 0; i < kSkeletonCount; ++i)
    {
        v.push_back(static_cast<float>(i) + 0.1f);
        v.push_back(static_cast<float>(i) + 0.2f);
        v.push_back(static_cast<float>(i) + 0.3f);
        v.push_back(0.1f);   // w
        v.push_back(0.2f);   // x
        v.push_back(0.3f);   // y
        v.push_back(0.4f);   // z
    }
    return v;
}

}  // namespace

/**
 * @test   DecodeAsGloveCSV.RejectsNullStream
 * @brief  异常：数据流指针为 NULL 时返回 -1，不得解引用。
 * @details 目的：UDP 收包失败时上层可能传入空缓冲，防护是函数契约的一部分。
 * @expected 返回值 == -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsNullStream)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];

    EXPECT_EQ(decodeAsGloveCSV_Func(nullptr, &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsNullHeader
 * @brief  异常：帧头指针为 NULL 时返回 -1，不得解引用。
 * @details 目的：调用方未初始化帧头时必须安全失败。
 * @expected 返回值 == -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsNullHeader)
{
    std::string frame = makeFrame(makeHeader(110, 1, 1, "euler"), makeEulerValues());
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], nullptr, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsNullSkeletonArray
 * @brief  异常：骨骼输出数组为 NULL 时返回 -1，不得解引用。
 * @details 目的：输出缓冲缺失时必须安全失败，防止写入野指针。
 * @expected 返回值 == -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsNullSkeletonArray)
{
    std::string frame = makeFrame(makeHeader(120, 1, 1, "euler"), makeEulerValues());
    StreamHeader sh;

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, nullptr, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsWrongSkeletonCount
 * @brief  异常：骨骼数量与协议不符（如 53 段人体协议）时返回 -1。
 * @details 目的：手部协议固定 42 段，传入其它数量说明调用方用错了协议对象。
 * @expected 返回值 == -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsWrongSkeletonCount)
{
    std::string frame = makeFrame(makeHeader(130, 1, 1, "euler"), makeEulerValues());
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, 53), -1);
}

/**
 * @test   DecodeAsGloveCSV.ParsesCompleteEulerFrame
 * @brief  正常路径：单子包 euler 帧完整解析，帧头与骨骼数据逐一落位。
 * @details 目的：这是协议的主链路——解析帧头、把 42 段数据从 CSV 落到
 *          SingleSkeletonDef、并把欧拉角换算为四元数。
 *          设计思路：数据值全段互异可检出串位；第 0 段骨骼欧拉角 (0,0,90)
 *          在 ZXY 顺序下有解析解 (0, 0, sin45°, cos45°)（金标准，非抄实现）。
 * @expected 返回 0；AvatarName/长度/帧号/姿态/顺序/坐标系与帧头一致；
 *           第 0 段与第 41 段骨骼位置、欧拉角与输入一致（容差 1e-3）；
 *           第 0 段四元数为 (0,0,0.7071,0.7071)（容差 1e-3）；骨骼名与索引正确。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, ParsesCompleteEulerFrame)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(100, 1, 1, "euler"), makeEulerValues());

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(sh.AvatarName)), "Glove1-SN-TEST");
    EXPECT_EQ(sh.AvatarNameLength, 14);
    EXPECT_EQ(sh.frameNumber, 100u);
    EXPECT_EQ(sh.skeletonPosition, SkeletonPosition_Meter);
    EXPECT_EQ(sh.skeletonAttitude, SkeletonAttitude_Euler);
    EXPECT_EQ(sh.channelOrder, MVSDK_ZXY);
    EXPECT_EQ(sh.skeletonCoordinate, SkeletonCoordinate_Relative);

    // 第 0 段骨骼（数据落位 + 解析解四元数）
    EXPECT_NEAR_FLOAT(ssd[0].position_meter[0], 0.1f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].position_meter[1], 0.2f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].position_meter[2], 0.3f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].euler_degree[0], 0.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].euler_degree[1], 0.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].euler_degree[2], 90.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[0], 0.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[1], 0.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[2], 0.7071068f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[3], 0.7071068f, 1e-3f);
    EXPECT_EQ(ssd[0].gloveSkeletonIndex, KHHS42_RightHand);
    EXPECT_EQ(std::string(ssd[0].skeletonName), "RightHand");

    // 第 41 段骨骼（尾部落位）
    EXPECT_NEAR_FLOAT(ssd[41].position_meter[0], 41.1f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[41].euler_degree[2], 41.6f, 1e-3f);
}

/**
 * @test   DecodeAsGloveCSV.ParsesQuatAttitudeFrame
 * @brief  正常路径：单子包 quat 帧解析，线上 wxyz 顺序转换为输出 xyzw。
 * @details 目的：钉死线上四元数字段顺序这一协议契约（wxyz 进、xyzw 出），
 *          防止后续重构时交换分量而无人察觉。
 *          设计思路：四元数 4 个分量取互异值 (0.1,0.2,0.3,0.4)，
 *          任何一次错误的顺序交换都会使断言变红。
 * @expected 返回 0；skeletonAttitude == Quat；ssd[0].quat_xyzw == (0.2, 0.3, 0.4, 0.1)。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, ParsesQuatAttitudeFrame)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(300, 1, 1, "quat"), makeQuatValues());

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sh.skeletonAttitude, SkeletonAttitude_Quat);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[0], 0.2f, 1e-4f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[1], 0.3f, 1e-4f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[2], 0.4f, 1e-4f);
    EXPECT_NEAR_FLOAT(ssd[0].quat_xyzw[3], 0.1f, 1e-4f);
}

/**
 * @test   DecodeAsGloveCSV.IncompleteSubpackageReturnsMinusOne
 * @brief  分帧：子包 1/2 到达但 2/2 未到时返回 -1，输出保持清零。
 * @details 目的：UDP 分包乱序/丢包是常态，不完整帧绝不能带着半份数据往外发。
 *          设计思路：只发前 21 段骨骼（半帧数据），断言返回值与输出缓冲状态。
 * @expected 返回 -1；ssd[0].position_meter 保持 memset 后的全 0。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, IncompleteSubpackageReturnsMinusOne)
{
    const std::vector<float> all = makeEulerValues();
    const std::vector<float> half(all.begin(),
                                  all.begin() + (KHHS42_SkeletonCount / 2) * kEulerFieldsPerSkeleton);
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(200, 1, 2, "euler"), half);

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, -1);
    EXPECT_NEAR_FLOAT(ssd[0].position_meter[0], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(ssd[0].position_meter[1], 0.0f, 1e-6f);
}

/**
 * @test   DecodeAsGloveCSV.TwoSubpackagesAssembleOneFrame
 * @brief  分帧：同一帧号的 1/2、2/2 两个子包拼成完整一帧并正确落位。
 * @details 目的：验证跨包数据段的拼接边界——前一子包以尾随逗号结尾，
 *          拼接后字段数恰好为 42 组，且各段数据不串位。
 *          设计思路：子包 1 放前 21 段、子包 2 放后 21 段，断言首尾两段
 *          的落位即可覆盖拼接点两侧。
 * @expected 子包 1 返回 -1；子包 2 返回 0；ssd[0] 来自子包 1、
 *           ssd[41] 来自子包 2（容差 1e-3）。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, TwoSubpackagesAssembleOneFrame)
{
    const std::vector<float> all = makeEulerValues();
    const size_t halfFields = (KHHS42_SkeletonCount / 2) * kEulerFieldsPerSkeleton;
    const std::vector<float> part1(all.begin(), all.begin() + static_cast<long>(halfFields));
    const std::vector<float> part2(all.begin() + static_cast<long>(halfFields), all.end());

    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];

    std::string frame1 = makeFrame(makeHeader(400, 1, 2, "euler"), part1);
    std::string frame2 = makeFrame(makeHeader(400, 2, 2, "euler"), part2);

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame1[0], &sh, ssd, KHHS42_SkeletonCount), -1);
    EXPECT_EQ(decodeAsGloveCSV_Func(&frame2[0], &sh, ssd, KHHS42_SkeletonCount), 0);

    EXPECT_NEAR_FLOAT(ssd[0].position_meter[0], 0.1f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[0].euler_degree[2], 90.0f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[41].position_meter[0], 41.1f, 1e-3f);
    EXPECT_NEAR_FLOAT(ssd[41].euler_degree[2], 41.6f, 1e-3f);
}

/**
 * @test   DecodeAsGloveCSV.WrongFieldValueCountReturnsMinusOne
 * @brief  分帧：数据段字段个数与 42 组不符时返回 -1。
 * @details 目的：字段数不符意味着帧被截断或协议不匹配，静默解析会错位污染。
 * @expected 返回 -1。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, WrongFieldValueCountReturnsMinusOne)
{
    std::vector<float> values = makeEulerValues();
    values.pop_back();  // 251 个字段，不等于 6 * 42
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(500, 1, 1, "euler"), values);

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.MissingSubpackageFieldReturnsMinusOne
 * @brief  分帧：帧头缺少 subpackage 字段时无法组帧，返回 -1。
 * @details 目的：subpackage 是组帧的触发条件，缺失说明数据源不是分帧协议。
 * @expected 返回 -1。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, MissingSubpackageFieldReturnsMinusOne)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = "Glove1-SN-TEST pos euler ZXY relative fn 600,"
                        + joinFloatsWithComma(makeEulerValues());

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}
/**
 * @test   DecodeAsGloveCSV.ParsesExplicitSkeleton42Marker
 * @brief  格式判定：帧头 `fn` 之后带 `skeleton_42` 标记时按 42 段解析。
 * @details 目的：标记是发送端声明布局的唯一权威，必须被消费并回报给调用方。
 * @expected 返回 0；skeletonFormat == GloveStreamFormat_Skeleton42；
 *           左手骨骼数 20；第 4 段为指尖末端 RightHandThumb3End。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, ParsesExplicitSkeleton42Marker)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(700, 1, 1, "euler", "skeleton_42"), makeEulerValues());

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sh.skeletonFormat, GloveStreamFormat_Skeleton42);
    EXPECT_EQ(sh.leftFigureSkeletonCount, 20);
    EXPECT_EQ(std::string(ssd[4].skeletonName), "RightHandThumb3End");
    EXPECT_EQ(ssd[4].gloveSkeletonIndex, KHHS42_RightHandThumb3End);
}

/**
 * @test   DecodeAsGloveCSV.ParsesUnmarked32Frame
 * @brief  格式判定：帧头无标记、列数为 32 组时按 32 段解析。
 * @details 目的：协议的默认格式是 32 段，必须能按列数识别出来。
 * @expected 返回 0；skeletonFormat == GloveStreamFormat_Skeleton32；
 *           左手骨骼数 15；第 4 段为 RightHandIndex1（32 段布局），末段为 RightHandPinky3。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, ParsesUnmarked32Frame)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(701, 1, 1, "euler"), makeEulerValues(KHHS32_SkeletonCount));

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sh.skeletonFormat, GloveStreamFormat_Skeleton32);
    EXPECT_EQ(sh.leftFigureSkeletonCount, 15);
    EXPECT_EQ(std::string(ssd[4].skeletonName), "RightHandIndex1");
    EXPECT_EQ(std::string(ssd[15].skeletonName), "RightHandPinky3");
    EXPECT_EQ(std::string(ssd[31].skeletonName), "LeftHandPinky3");
}

/**
 * @test   DecodeAsGloveCSV.Unmarked42FrameStillTreatedAs42
 * @brief  兼容：现网 42 段帧没有标记，仍须按 42 段解析。
 * @details 目的：若严格按「无标记即 32」处理，今天可用的 42 段数据会被拒收；
 *          列数消歧是保证向后兼容的关键。
 * @expected 返回 0；skeletonFormat == GloveStreamFormat_Skeleton42；左手骨骼数 20。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, Unmarked42FrameStillTreatedAs42)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(702, 1, 1, "euler"), makeEulerValues());

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sh.skeletonFormat, GloveStreamFormat_Skeleton42);
    EXPECT_EQ(sh.leftFigureSkeletonCount, 20);
}

/**
 * @test   DecodeAsGloveCSV.RejectsOpenXRMarker
 * @brief  格式判定：`skeleton_openxr` 布局未定义，整帧丢弃并回报专用返回码。
 * @details 目的：OpenXR 与 32/42 不是一一对应，猜着解析会静默错位，必须显式拒绝。
 * @expected 返回 MGSDK_DECODE_OPENXR_UNSUPPORTED；
 *           sh.skeletonFormat == GloveStreamFormat_OpenXR。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsOpenXRMarker)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(703, 1, 1, "euler", "skeleton_openxr"), makeEulerValues());

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, MGSDK_DECODE_OPENXR_UNSUPPORTED);
    EXPECT_EQ(sh.skeletonFormat, GloveStreamFormat_OpenXR);
}

/**
 * @test   DecodeAsGloveCSV.RejectsUnknownSkeletonMarker
 * @brief  格式判定：未知的 `skeleton_*` 标记整帧丢弃，绝不回退成 32 段。
 * @details 目的：把未知布局当 32 段解析不会报错，只会静默错位，必须拒收。
 * @expected 返回 -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsUnknownSkeletonMarker)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(704, 1, 1, "euler", "skeleton_53"), makeEulerValues());

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsMarkerContradictingColumnCount
 * @brief  格式判定：标记声明 32 段却给了 42 组列，整帧丢弃。
 * @details 目的：显式标记优先于列数推断，二者矛盾说明帧损坏或发送端异常。
 * @expected 返回 -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsMarkerContradictingColumnCount)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(705, 1, 1, "euler", "skeleton_32"), makeEulerValues());

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsWhenCapacityBelowDetectedCount
 * @brief  容量：调用方只提供 32 段容量，却收到 42 段帧，必须拒绝而不是越界写。
 * @details 目的：skeletonCount 语义是输出数组容量，超容量必须失败。
 * @expected 返回 -1。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, RejectsWhenCapacityBelowDetectedCount)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(706, 1, 1, "euler", "skeleton_42"), makeEulerValues());

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS32_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsUnmarkedCountMatchingNeither
 * @brief  格式判定：无标记且列数既不等于 32 组也不等于 42 组时丢弃。
 * @details 目的：30 组是人为构造的第三形态，必须走拒收路径而不是被就近归类。
 * @expected 返回 -1。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, RejectsUnmarkedCountMatchingNeither)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(makeHeader(707, 1, 1, "euler"), makeEulerValues(30));

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.RejectsInconsistentMarkerAcrossSubpackages
 * @brief  分帧：同一帧号的两个子包声明了不同格式时整帧丢弃。
 * @details 目的：分包都带同一个帧头，标记不一致说明串帧或发送端异常，
 *          把两段数据拼起来会得到错位帧。
 * @expected 两个子包的调用都返回 -1。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, RejectsInconsistentMarkerAcrossSubpackages)
{
    const std::vector<float> all = makeEulerValues(KHHS32_SkeletonCount);
    const size_t halfFields = (KHHS32_SkeletonCount / 2) * kEulerFieldsPerSkeleton;
    const std::vector<float> part1(all.begin(), all.begin() + static_cast<long>(halfFields));
    const std::vector<float> part2(all.begin() + static_cast<long>(halfFields), all.end());

    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];

    std::string frame1 = makeFrame(makeHeaderWithMarker(708, 1, 2, "euler", "skeleton_32"), part1);
    std::string frame2 = makeFrame(makeHeaderWithMarker(708, 2, 2, "euler", "skeleton_42"), part2);

    EXPECT_EQ(decodeAsGloveCSV_Func(&frame1[0], &sh, ssd, KHHS42_SkeletonCount), -1);
    EXPECT_EQ(decodeAsGloveCSV_Func(&frame2[0], &sh, ssd, KHHS42_SkeletonCount), -1);
}

/**
 * @test   DecodeAsGloveCSV.Skeleton32FillsOnlyThirtyTwoSkeletons
 * @brief  落位：32 段帧只写前 32 项，第 33 项起保持调用方给的初值。
 * @details 目的：32 段容器只有 32 项，解码器不得越界写到第 33 项。
 * @expected 返回 0；ssd[31] 为 RightHandPinky3；ssd[32] 的名字为空。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, Skeleton32FillsOnlyThirtyTwoSkeletons)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(709, 1, 1, "euler", "skeleton_32"), makeEulerValues(KHHS32_SkeletonCount));

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(std::string(ssd[15].skeletonName), "RightHandPinky3");
    EXPECT_EQ(std::string(ssd[31].skeletonName), "LeftHandPinky3");
    EXPECT_EQ(ssd[32].skeletonName[0], '\0');
}

/**
 * @test   DecodeAsGloveCSV.Skeleton32Uses32IndexTable
 * @brief  落位：32 段帧的骨骼名与索引取自 32 段表，而不是 42 段表。
 * @details 目的：两份表的顺序不同，用错表会得到完全错误的名字与索引。
 * @expected ssd[0].gloveSkeletonIndex32 == KHHS32_RightHand；
 *           ssd[15].gloveSkeletonIndex32 == KHHS32_RightHandPinky3。
 * @priority P0
 */
TEST(DecodeAsGloveCSV, Skeleton32Uses32IndexTable)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        makeHeaderWithMarker(710, 1, 1, "euler", "skeleton_32"), makeEulerValues(KHHS32_SkeletonCount));

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ssd[0].gloveSkeletonIndex32, KHHS32_RightHand);
    EXPECT_EQ(ssd[15].gloveSkeletonIndex32, KHHS32_RightHandPinky3);
}

/**
 * @test   DecodeAsGloveCSV.ActorNameResemblingMarkerIsNotTreatedAsMarker
 * @brief  格式判定：演员名恰好叫 `skeleton_42` 时不得被误认成格式标记。
 * @details 目的：标记位置固定在 `fn` 之后，检测只看该位置；把演员名当标记
 *          会让整帧按错误布局解析。
 * @expected 返回 0；skeletonFormat == GloveStreamFormat_Skeleton32（无标记，按列数判定）。
 * @priority P1
 */
TEST(DecodeAsGloveCSV, ActorNameResemblingMarkerIsNotTreatedAsMarker)
{
    StreamHeader sh;
    SingleSkeletonDef ssd[KHHS42_SkeletonCount];
    std::string frame = makeFrame(
        "skeleton_42 pos euler ZXY relative fn 711 subpackage 1/1",
        makeEulerValues(KHHS32_SkeletonCount));

    const int ret = decodeAsGloveCSV_Func(&frame[0], &sh, ssd, KHHS42_SkeletonCount);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sh.skeletonFormat, GloveStreamFormat_Skeleton32);
}
/// @endcond
