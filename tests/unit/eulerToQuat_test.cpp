/**
 * @file  eulerToQuat_test.cpp
 * @brief 欧拉角转四元数（EulerToQuat）单元测试，框架为 GoogleTest。
 *
 * 覆盖策略
 *  - 解析解（金标准）：0° 输入 → 单位四元数；单轴 90° → (sin45°, 0, 0, cos45°)，
 *    期望值全部由旋转矩阵 / 四元数定义手算得出，不复制实现公式；
 *  - 不变量：任意角度、全部 6 种旋转顺序（XYZ/XZY/YXZ/YZX/ZXY/ZYX）下，
 *    输出四元数模长恒为 1；
 *  - 异常场景：NaN / Inf 输入不得污染输出缓冲（拒绝并保持原值）；
 *    非法旋转顺序与空输出指针不得引发未定义行为。
 *
 * 该层为纯计算测试，无窗口、无 GPU 依赖，毫秒级执行。
 */
#include "inc/eulerToQuat.h"

#include <cmath>
#include <limits>

#include "gtest_assertions.h"

// TEST 宏展开体不参与 Doxygen 文档化（避免 WARN_NO_PARAMDOC 逐个误报）
/// @cond DOC_HIDDEN
namespace {

/// 测试用非法旋转顺序取值：越出 MVSDK_ChannelOrder 合法范围 [0, MVSDK_CHANNEL_ORDER_MAX)。
constexpr MVSDK_ChannelOrder kInvalidOrder =
    static_cast<MVSDK_ChannelOrder>(MVSDK_CHANNEL_ORDER_MAX + 1);

/// 全部 6 种合法旋转顺序，供循环用例遍历。
const MVSDK_ChannelOrder kAllOrders[] = {
    MVSDK_XYZ, MVSDK_XZY, MVSDK_YXZ, MVSDK_YZX, MVSDK_ZXY, MVSDK_ZYX
};

}  // namespace

/**
 * @test   EulerToQuat.ZeroDegreesYieldsIdentityForAllOrders
 * @brief  解析解：6 种旋转顺序下 0° 输入都应输出单位四元数 (0,0,0,1)。
 * @details 目的：零旋转是 SDK 全链路的默认值，若被污染会放大成整只手套的初始姿态错误。
 *          设计思路：不传默认顺序而是遍历全部 6 种顺序，防止某个分支的矩阵构造
 *          在零角度下碰巧不一致。
 * @expected 每种顺序下 q == (0,0,0,1)，容差 1e-6。
 * @priority P0
 */
TEST(EulerToQuat, ZeroDegreesYieldsIdentityForAllOrders)
{
    for (MVSDK_ChannelOrder order : kAllOrders)
    {
        SCOPED_TRACE(::testing::Message() << "order=" << order);
        float q[4] = {-9.0f, -9.0f, -9.0f, -9.0f};

        EulerToQuat(0.0f, 0.0f, 0.0f, order, q);

        EXPECT_NEAR_FLOAT(q[0], 0.0f, 1e-6f);
        EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-6f);
        EXPECT_NEAR_FLOAT(q[2], 0.0f, 1e-6f);
        EXPECT_NEAR_FLOAT(q[3], 1.0f, 1e-6f);
    }
}

/**
 * @test   EulerToQuat.ZAxis90ProducesAnalyticQuaternion
 * @brief  解析解：ZXY 顺序 (0,0,90) 应输出绕 Z 轴 +90° 的四元数。
 * @details 目的：验证 ZXY 分支矩阵构造与 rotation2quat 的符号约定。
 *          设计思路：期望值手算——绕 Z 轴 +90° 的四元数为 (x,y,z,w) =
 *          (0, 0, sin45°, cos45°) = (0, 0, 0.7071068, 0.7071068)，
 *          与实现内部公式无关，属金标准。
 * @expected q == (0, 0, 0.7071068, 0.7071068)，容差 1e-4。
 * @priority P0
 */
TEST(EulerToQuat, ZAxis90ProducesAnalyticQuaternion)
{
    float q[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    EulerToQuat(0.0f, 0.0f, 90.0f, MVSDK_ZXY, q);

    EXPECT_NEAR_FLOAT(q[0], 0.0f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[2], 0.7071068f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[3], 0.7071068f, 1e-4f);
}

/**
 * @test   EulerToQuat.XAxis90ProducesAnalyticQuaternion
 * @brief  解析解：XYZ 顺序 (90,0,0) 应输出绕 X 轴 +90° 的四元数。
 * @details 目的：与 Z 轴用例互补，覆盖 XYZ 分支并验证 X 分量符号。
 *          设计思路：绕 X 轴 +90° 的四元数 (x,y,z,w) = (sin45°, 0, 0, cos45°)。
 * @expected q == (0.7071068, 0, 0, 0.7071068)，容差 1e-4。
 * @priority P0
 */
TEST(EulerToQuat, XAxis90ProducesAnalyticQuaternion)
{
    float q[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    EulerToQuat(90.0f, 0.0f, 0.0f, MVSDK_XYZ, q);

    EXPECT_NEAR_FLOAT(q[0], 0.7071068f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[2], 0.0f, 1e-4f);
    EXPECT_NEAR_FLOAT(q[3], 0.7071068f, 1e-4f);
}

/**
 * @test   EulerToQuat.QuaternionNormIsOneForAllOrders
 * @brief  不变量：任意角度与任意顺序下输出四元数模长恒为 1。
 * @details 目的：下游按单位四元数参与姿态合成，模长漂移会直接造成骨骼缩放畸变。
 *          设计思路：取一组非平凡角度 (30,40,50) 遍历全部 6 种顺序；
 *          模长是与旋转顺序无关的硬约束。
 * @expected 对全部顺序 |q| 与 1 的偏差 <= 1e-4。
 * @priority P0
 */
TEST(EulerToQuat, QuaternionNormIsOneForAllOrders)
{
    for (MVSDK_ChannelOrder order : kAllOrders)
    {
        SCOPED_TRACE(::testing::Message() << "order=" << order);
        float q[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        EulerToQuat(30.0f, 40.0f, 50.0f, order, q);

        const float norm = std::sqrt(q[0] * q[0] + q[1] * q[1] +
                                     q[2] * q[2] + q[3] * q[3]);
        EXPECT_NEAR_FLOAT(norm, 1.0f, 1e-4f);
    }
}

/**
 * @test   EulerToQuat.RejectsNonFiniteInputKeepsOutput
 * @brief  异常：NaN / Inf 输入应被拒绝，输出缓冲保持调用前的内容。
 * @details 目的：传感器异常时会产出非有限角度，若 NaN 进入四元数会沿姿态链路
 *          静默扩散到整只手套（NaN 参与任何比较都是 false，难以察觉）。
 *          设计思路：按工程统一约定"非有限值 → 拒绝并保持原值"——预先把输出
 *          初始化为单位四元数，断言调用后仍是单位四元数；用例在实现补防护前为断言红。
 * @expected 两次非法调用后 q 均保持 (0,0,0,1)，容差 1e-6。
 * @priority P0
 */
TEST(EulerToQuat, RejectsNonFiniteInputKeepsOutput)
{
    const float kNaN = std::numeric_limits<float>::quiet_NaN();
    const float kInf = std::numeric_limits<float>::infinity();
    float q[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    EulerToQuat(kNaN, 0.0f, 0.0f, MVSDK_ZXY, q);

    EXPECT_NEAR_FLOAT(q[0], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[2], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[3], 1.0f, 1e-6f);

    EulerToQuat(0.0f, kInf, 0.0f, MVSDK_ZXY, q);

    EXPECT_NEAR_FLOAT(q[0], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[2], 0.0f, 1e-6f);
    EXPECT_NEAR_FLOAT(q[3], 1.0f, 1e-6f);
}

/**
 * @test   EulerToQuat.InvalidChannelOrderYieldsIdentity
 * @brief  异常：越界的旋转顺序枚举不得读取未初始化内存，应输出单位四元数。
 * @details 目的：原实现在 default 分支直接落到 rotation2quat，而 RotMatrix
 *          未初始化——读到栈上垃圾后输出 NaN/随机值，属未定义行为。
 *          设计思路：用越界枚举值（MAX+1）触发该分支，断言输出全部有限
 *          且等于单位四元数（安全中性值，不污染下游姿态）。
 * @expected 输出 q 全部有限，且 == (0,0,0,1)，容差 1e-5。
 * @priority P0
 */
TEST(EulerToQuat, InvalidChannelOrderYieldsIdentity)
{
    float q[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    EulerToQuat(30.0f, 40.0f, 50.0f, kInvalidOrder, q);

    EXPECT_TRUE(std::isfinite(q[0]) && std::isfinite(q[1]) &&
                std::isfinite(q[2]) && std::isfinite(q[3]));
    EXPECT_NEAR_FLOAT(q[0], 0.0f, 1e-5f);
    EXPECT_NEAR_FLOAT(q[1], 0.0f, 1e-5f);
    EXPECT_NEAR_FLOAT(q[2], 0.0f, 1e-5f);
    EXPECT_NEAR_FLOAT(q[3], 1.0f, 1e-5f);
}

/**
 * @test   EulerToQuat.NullOutputPointerDoesNotCrash
 * @brief  异常：输出指针为 NULL 时函数应直接返回，不得崩溃。
 * @details 目的：上层解帧代码在异常路径下可能传入空缓冲，原实现直接解引用
 *          fData[0..3] 会段错误。防御性检查是 SDK 的红线约定。
 *          设计思路：单独成用例——崩溃类缺陷会终止整个测试进程，
 *          gtest_discover_tests 按用例拆分 ctest 条目后只影响本条。
 * @expected 调用正常返回（不崩溃、不断言失败）。
 * @priority P1
 */
TEST(EulerToQuat, NullOutputPointerDoesNotCrash)
{
    EulerToQuat(0.0f, 0.0f, 0.0f, MVSDK_ZXY, nullptr);
    SUCCEED();
}
/// @endcond
