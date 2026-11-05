/**
 * @file  motionGloveSDK_example_vtk.cpp
 * @brief 示例 6：基于 VTK 9 的手套骨骼三维可视化（骨骼点 + 连线成手型）。
 * @ingroup mgsdk_examples
 *
 * @details
 * 渲染拓扑与 MotionGloveSDK/include/MOTIONGLOVE_SDK_STREAM_FORMAT_2026-11-02.md 一致：
 *  - 每段骨骼的起始点画成小球（骨骼点）；
 *  - 相邻骨骼点之间用连线连接，组成手型骨架。
 *
 * 位姿来源按帧头 hdr.skeletonFormat 区分（见规范 3.2 / 3.3 节）：
 *  - 42 段（GloveStreamFormat_Skeleton42）：每根手指带独立指尖末端点（*End），直接取用帧数据；
 *  - 32 段（GloveStreamFormat_Skeleton32）：没有指尖末端点，虚拟指尖的**朝向取该指第三段
 *    骨骼的姿态**——用第三段的全局四元数旋转骨骼静止轴向（局部 +Y），再沿该方向延伸
 *    20 mm 得到指尖点；无姿态数据时退化为沿第 2→第 3 节方向延伸。
 *    注意不能简单地"沿上一节延长"，那会让末节与上一节共线、手指看起来少一个弯曲。
 *
 * 两种格式最终都映射到同一套 42 点布局（槽位顺序即 KHHS42_*），切换格式只更新坐标，
 * 不需要重建几何。
 *
 * 场景元素（与 Python 版 motionGloveSDK_example3_3dView.py 保持一致，左右面板控件暂不实现）：
 *  - 手部骨骼：42 个关节小球 + 小球之间的连线（左右手分别着色）；
 *  - 地平面网格（XZ 平面，y = 0）；
 *  - 场景中间（原点）的三色坐标系 vtkAxesActor（X 红 / Y 绿 / Z 蓝，带轴标注）；
 *  - 窗口左上角的状态叠加文字（vtkCornerAnnotation）：UDP 端口是监听中还是被占用、
 *    当前骨骼段数模式（32 段 / 42 段，随主机端动态切换）、最新帧号，三者每帧刷新；
 *  - 相机初始视角与 Python 版 setup_camera 相同。
 *
 * 命令行参数：
 *   --self-test           只执行初始化（UDP 绑定 + 场景搭建）后退出，用于 CI 启动自检。
 *   --headless &lt;秒&gt;       不开窗口，只按 ~60Hz 取帧并打印诊断日志（验证数据链路）。
 *   --render-check &lt;秒&gt;   离屏渲染自检：不弹窗，每秒存一张 PNG 到 --render-out 目录，
 *                         用于客观验证"数据在变、画面也在变"。
 *   --render-out &lt;目录&gt;   render-check 的 PNG 输出目录（默认当前目录）。
 *   --dump &lt;文件&gt;         把收到的原始 32 段帧写成 CSV（fn + 32 x [pos xyz + quat xyzw]），
 *                         用于离线核对姿态与位置的参考系（默认导出 300 帧）。
 *   --dump-frames &lt;n&gt;     --dump 的最大导出帧数。
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "motionGloveSDK.h"
#include "motionGloveSDK_version.h"

#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCaptionActor2D.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkCornerAnnotation.h>
#include <vtkGlyph3DMapper.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPNGWriter.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkWindowToImageFilter.h>

/** @brief 默认绑定的 UDP 端口号。 */
static const int kDefaultUdpPort = 5001;
/** @brief 默认查询的数据流（手套）名称。 */
static const char* kDefaultGloveName = "Glove1";

/** @brief 统一渲染布局的关节数：与 42 段骨骼一致（右手 21 + 左手 21，含 10 个指尖末端）。 */
static const int kJointCount = KHHS42_SkeletonCount;
/** @brief 左手首个槽位下标（右手占 0~20，左手占 21~41）。 */
static const int kLeftHandBaseSlot = KHHS42_LeftHand;

/** @brief 骨骼点小球半径，单位米（与 Python 版一致）。 */
static const double kJointRadiusMeter = 0.005;
/** @brief 32 段格式下虚拟指尖末端的骨骼长度，单位米（与 Python 版 _VIRTUAL_TIP_LENGTH_M 一致）。 */
static const double kVirtualTipLengthMeter = 0.020;
/** @brief 骨骼静止姿态下的局部轴向：指尖方向由第三段的姿态旋转该轴得到（与 Python 版一致）。 */
static const double kBoneLocalAxis[3] = { 0.0, 1.0, 0.0 };

/** @brief 地平面网格的格距，单位米（与 Python 版一致）。 */
static const double kGridCellMeter = 0.05;
/** @brief 地平面网格的半边长，单位米（与 Python 版一致，±30 cm）。 */
static const double kGridHalfExtentMeter = 0.30;
/** @brief 场景中间（原点）坐标系的总长度，单位米（与 Python 版一致）。 */
static const double kAxesLengthMeter = 0.025;

/**
 * @brief 32 段骨骼索引 → 统一布局槽位（槽位顺序同 KHHS42_*）。
 * @details 32 段顺序见规范 3.2 节（右手 16 段 → 左手 16 段）；每根手指只有 3 节，
 *          因此跳过 42 段布局里每个 *End 槽位（4/8/12/16/20 与 25/29/33/37/41）。
 */
static const int kSlotOfSkeleton32[KHHS32_SkeletonCount] = {
    // 右手：手掌、拇指 1/2/3、食指 1/2/3、中指 1/2/3、无名指 1/2/3、小指 1/2/3
    0, 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, 17, 18, 19,
    // 左手：同样的排列，整体平移 21 个槽位
    21, 22, 23, 24, 26, 27, 28, 30, 31, 32, 34, 35, 36, 38, 39, 40,
};

/** @brief 统一布局里的 10 个指尖末端槽位（右手 5 个 + 左手 5 个）。 */
static const int kTipSlots[] = { 4, 8, 12, 16, 20, 25, 29, 33, 37, 41 };
/** @brief 指尖末端槽位个数。 */
static const int kTipSlotCount = static_cast<int>(sizeof(kTipSlots) / sizeof(kTipSlots[0]));

/**
 * @brief 统一布局（42 槽位）中每个槽位的父骨骼槽位，-1 表示该槽位为手的根（手掌）。
 * @details 用于把 32 段帧里的**局部（相对父骨骼）**四元数沿父链累乘成全局姿态，
 *          从而得到第三段骨骼的全局朝向。槽位编号规律：父槽位下标恒小于子槽位，
 *          因此按槽位升序遍历即可保证先算父再算子。
 */
static const int kParentSlot[kJointCount] = {
    -1,                                                  // 右手：手掌
    0, 1, 2, 3,                                          // 右手拇指 1/2/3/End
    0, 5, 6, 7,                                          // 右手食指
    0, 9, 10, 11,                                        // 右手中指
    0, 13, 14, 15,                                       // 右手无名指
    0, 17, 18, 19,                                       // 右手小指
    -1,                                                  // 左手：手掌（根，无父骨骼）
    21, 22, 23, 24,                                      // 左手拇指 1/2/3/End
    21, 26, 27, 28,                                      // 左手食指
    21, 30, 31, 32,                                      // 左手中指
    21, 34, 35, 36,                                      // 左手无名指
    21, 38, 39, 40,                                      // 左手小指
};

/**
 * @brief 骨骼连线（统一布局槽位对）：手掌 → 五指根部，然后每根手指逐节连到指尖末端。
 * @details 共 40 条：每手 5 条掌骨连线 + 5 指 x 3 条指节连线。
 */
static const int kBoneEdges[][2] = {
    // 右手：手掌连到五指第 1 节
    { 0, 1 }, { 0, 5 }, { 0, 9 }, { 0, 13 }, { 0, 17 },
    // 右手：拇指 / 食指 / 中指 / 无名指 / 小指，逐节相连直到指尖末端
    { 1, 2 }, { 2, 3 }, { 3, 4 },
    { 5, 6 }, { 6, 7 }, { 7, 8 },
    { 9, 10 }, { 10, 11 }, { 11, 12 },
    { 13, 14 }, { 14, 15 }, { 15, 16 },
    { 17, 18 }, { 18, 19 }, { 19, 20 },
    // 左手：与右手同构，槽位整体 +21
    { 21, 22 }, { 21, 26 }, { 21, 30 }, { 21, 34 }, { 21, 38 },
    { 22, 23 }, { 23, 24 }, { 24, 25 },
    { 26, 27 }, { 27, 28 }, { 28, 29 },
    { 30, 31 }, { 31, 32 }, { 32, 33 },
    { 34, 35 }, { 35, 36 }, { 36, 37 },
    { 38, 39 }, { 39, 40 }, { 40, 41 },
};
/** @brief 骨骼连线数量。 */
static const int kBoneEdgeCount = static_cast<int>(sizeof(kBoneEdges) / sizeof(kBoneEdges[0]));

/**
 * @brief 判断统一布局里的某个槽位是否是指尖末端。
 * @param slot [in] 槽位下标。
 * @return true 表示指尖末端（32 段格式下需要外推）。
 */
static bool IsTipSlot(int slot)
{
    for (int i = 0; i < kTipSlotCount; ++i)
    {
        if (kTipSlots[i] == slot)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief 构建未收到有效位置数据前使用的"手掌状"默认布局坐标。
 * @return 42 个槽位（含指尖末端）的默认位置，单位米。
 */
static std::array<std::array<double, 3>, kJointCount> BuildFallbackPositions()
{
    // 先用一个近似手掌的布局，等收到有效 position_meter 再被真实坐标替换。
    std::array<std::array<double, 3>, kJointCount> p{};

    auto setHand = [&](int baseSlot, double baseX, double baseY, double baseZ, double thumbSide)
        {
            p[baseSlot] = { baseX, baseY, baseZ };

            auto setFinger = [&](int i1, double fx, double fy)
                {
                    p[i1] = { baseX + fx, baseY + fy, baseZ };
                    p[i1 + 1] = { baseX + fx, baseY + fy + 0.025, baseZ };
                    p[i1 + 2] = { baseX + fx, baseY + fy + 0.050, baseZ };
                    p[i1 + 3] = { baseX + fx, baseY + fy + 0.075, baseZ };
                };

            // 拇指 / 食指 / 中指 / 无名指 / 小指，每个 4 个点（3 节 + 指尖末端）
            setFinger(baseSlot + 1, 0.030 * thumbSide, -0.010);
            setFinger(baseSlot + 5, 0.020, 0.020);
            setFinger(baseSlot + 9, 0.000, 0.025);
            setFinger(baseSlot + 13, -0.020, 0.020);
            setFinger(baseSlot + 17, -0.040, 0.015);
        };

    setHand(0, 0.080, 0.000, 0.000, +1.0);
    setHand(kLeftHandBaseSlot, -0.080, 0.000, 0.000, -1.0);

    return p;
}

/**
 * @brief 归一化三维向量。
 * @param v [in,out] 待归一化的向量，原位修改。
 * @return true 表示归一化成功；false 表示向量长度过小（方向退化）。
 */
static bool NormalizeVector(double v[3])
{
    const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len < 1e-9)
    {
        return false;
    }
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
    return true;
}

/**
 * @brief 归一化四元数（分量顺序 xyzw）。
 * @param q [in,out] 待归一化的四元数，原位修改。
 * @return true 表示归一化成功；false 表示模长过小（非法四元数，例如全零）。
 */
static bool NormalizeQuaternion(double q[4])
{
    const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len < 1e-6)
    {
        return false;
    }
    for (int i = 0; i < 4; ++i)
    {
        q[i] /= len;
    }
    return true;
}

/**
 * @brief 四元数乘法：out = a * b（分量顺序 xyzw），用于把局部旋转累乘成全局旋转。
 * @param a [in] 左操作数（父骨骼的全局旋转）。
 * @param b [in] 右操作数（子骨骼的局部旋转）。
 * @param out [out] 结果四元数。
 */
static void MultiplyQuaternion(const double a[4], const double b[4], double out[4])
{
    const double ax = a[0], ay = a[1], az = a[2], aw = a[3];
    const double bx = b[0], by = b[1], bz = b[2], bw = b[3];
    out[0] = aw * bx + ax * bw + ay * bz - az * by;
    out[1] = aw * by - ax * bz + ay * bw + az * bx;
    out[2] = aw * bz + ax * by - ay * bx + az * bw;
    out[3] = aw * bw - ax * bx - ay * by - az * bz;
}

/**
 * @brief 用四元数旋转三维向量：out = q * v * q⁻¹（分量顺序 xyzw）。
 * @param q [in] 旋转四元数（已归一化）。
 * @param v [in] 待旋转向量。
 * @param out [out] 旋转后的向量。
 */
static void RotateVectorByQuaternion(const double q[4], const double v[3], double out[3])
{
    const double qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    const double vx = v[0], vy = v[1], vz = v[2];

    const double tx = 2.0 * (qy * vz - qz * vy);
    const double ty = 2.0 * (qz * vx - qx * vz);
    const double tz = 2.0 * (qx * vy - qy * vx);

    out[0] = vx + qw * tx + (qy * tz - qz * ty);
    out[1] = vy + qw * ty + (qz * tx - qx * tz);
    out[2] = vz + qw * tz + (qx * ty - qy * tx);
}

/**
 * @brief 计算 32 段骨骼格式下虚拟指尖末端的位置。
 * @details 指尖末节骨骼的**朝向由第三段骨骼的姿态决定**：把骨骼静止轴向（局部 +Y）
 *          用第三段的全局四元数旋转，得到末节骨骼方向，再沿该方向延伸固定长度
 *          %kVirtualTipLengthMeter。这样末节与上一节不再共线（不再是"沿上一节直接延长"），
 *          与 Python 版 compute_frame_pose() 的处理完全一致。
 *          没有有效姿态数据（未开启姿态输出 / 四元数非法）时，退化为沿 P2→P3 方向延伸，
 *          保证仍能画出指尖。
 * @param p2 [in] 第 2 节位置（方向退化时的备用方向起点）。
 * @param p3 [in] 第 3 节位置（指尖末端的起点）。
 * @param thirdGlobalQuat [in] 第三段骨骼的全局四元数（xyzw）。
 * @param hasQuat [in] 第三段骨骼四元数是否有效。
 * @param outTip [out] 计算得到的指尖末端位置。
 */
static void ComputeVirtualTipPosition(const double p2[3], const double p3[3],
                                      const double thirdGlobalQuat[4], bool hasQuat,
                                      double outTip[3])
{
    double direction[3] = { 0.0, 0.0, 0.0 };
    bool directionValid = false;

    if (hasQuat)
    {
        RotateVectorByQuaternion(thirdGlobalQuat, kBoneLocalAxis, direction);
        directionValid = NormalizeVector(direction);
    }

    if (!directionValid)
    {
        // 无姿态数据时退化：沿第三段骨段的实际方向（P2 → P3）延伸。
        direction[0] = p3[0] - p2[0];
        direction[1] = p3[1] - p2[1];
        direction[2] = p3[2] - p2[2];
        directionValid = NormalizeVector(direction);
    }

    if (!directionValid)
    {
        // 三点重合，方向完全退化：沿 +Y 延伸一个固定长度。
        outTip[0] = p3[0];
        outTip[1] = p3[1] + kVirtualTipLengthMeter;
        outTip[2] = p3[2];
        return;
    }

    outTip[0] = p3[0] + direction[0] * kVirtualTipLengthMeter;
    outTip[1] = p3[1] + direction[1] * kVirtualTipLengthMeter;
    outTip[2] = p3[2] + direction[2] * kVirtualTipLengthMeter;
}

/**
 * @brief 把 32 段帧里各骨骼的四元数换算成统一布局下的**全局**姿态。
 * @details 帧头 skeletonCoordinate 为 Relative 时，局部旋转需沿父链左乘累乘：
 *          global = parent_global * local（与 Python 版约定一致）；为 Global 时直接取用。
 *          没有姿态数据（skeletonAttitude == None）或四元数非法（例如全零）的槽位
 *          标记为无效，由调用方决定退化处理。
 * @param frame [in] 32 段骨骼帧。
 * @param globalQuats [out] 按统一布局槽位索引存放的全局四元数（xyzw）。
 * @param quatValid [out] 各槽位四元数是否有效。
 */
static void BuildGlobalQuaternions(const KHHS32PosAttitude_TypeDef& frame,
                                   std::array<std::array<double, 4>, kJointCount>& globalQuats,
                                   std::array<bool, kJointCount>& quatValid)
{
    const bool hasAttitude = (frame.hdr.skeletonAttitude != SkeletonAttitude_None);
    const bool attitudeIsGlobal = (frame.hdr.skeletonCoordinate == SkeletonCoordinate_Global);

    for (int i = 0; hasAttitude && i < KHHS32_SkeletonCount; ++i)
    {
        const int slot = kSlotOfSkeleton32[i];
        // SDK 内部四元数顺序为 xyzw（见 SingleSkeletonDef::quat_xyzw）。
        globalQuats[slot][0] = static_cast<double>(frame.skeletons[i].quat_xyzw[0]);
        globalQuats[slot][1] = static_cast<double>(frame.skeletons[i].quat_xyzw[1]);
        globalQuats[slot][2] = static_cast<double>(frame.skeletons[i].quat_xyzw[2]);
        globalQuats[slot][3] = static_cast<double>(frame.skeletons[i].quat_xyzw[3]);
        quatValid[slot] = NormalizeQuaternion(globalQuats[slot].data());
    }

    if (attitudeIsGlobal)
    {
        return;   // 已经是全局姿态，无需累乘
    }

    // 局部 → 全局：按槽位升序遍历，父槽位下标恒小于子槽位，保证先算父再算子。
    for (int slot = 0; slot < kJointCount; ++slot)
    {
        if (!quatValid[slot])
        {
            continue;
        }
        const int parent = kParentSlot[slot];
        if (parent < 0 || !quatValid[parent])
        {
            continue;   // 根骨骼或父链断裂：局部即全局
        }
        double composed[4] = { 0.0, 0.0, 0.0, 0.0 };
        MultiplyQuaternion(globalQuats[parent].data(), globalQuats[slot].data(), composed);
        for (int k = 0; k < 4; ++k)
        {
            globalQuats[slot][k] = composed[k];
        }
    }
}

/**
 * @brief 构建 XZ 平面（y = 0）上的地平面网格线。
 * @details 网格间距 0.05 m，范围 X/Z 各 30 cm（-0.30 m ~ +0.30 m），
 *          共 13 x 13 条线段（与 Python 版 ground_plane 参数一致）。
 * @return 只包含线段单元的网格 vtkPolyData。
 */
static vtkSmartPointer<vtkPolyData> BuildGroundGrid()
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();

    const int lineCount = static_cast<int>(std::lround((2.0 * kGridHalfExtentMeter) / kGridCellMeter)) + 1;

    for (int i = 0; i < lineCount; ++i)
    {
        const double offset = -kGridHalfExtentMeter + i * kGridCellMeter;

        // 沿 Z 方向的一条线
        const vtkIdType zStart = points->InsertNextPoint(offset, 0.0, -kGridHalfExtentMeter);
        const vtkIdType zEnd = points->InsertNextPoint(offset, 0.0, kGridHalfExtentMeter);
        const vtkIdType zLine[2] = { zStart, zEnd };
        lines->InsertNextCell(2, zLine);

        // 沿 X 方向的一条线
        const vtkIdType xStart = points->InsertNextPoint(-kGridHalfExtentMeter, 0.0, offset);
        const vtkIdType xEnd = points->InsertNextPoint(kGridHalfExtentMeter, 0.0, offset);
        const vtkIdType xLine[2] = { xStart, xEnd };
        lines->InsertNextCell(2, xLine);
    }

    auto grid = vtkSmartPointer<vtkPolyData>::New();
    grid->SetPoints(points);
    grid->SetLines(lines);
    return grid;
}

/**
 * @brief 创建场景中间（原点）的坐标系 actor。
 * @details 与 Python 版 vtk_axes.add_axes_to_renderer 一致：vtkAxesActor，
 *          圆柱轴杆 + 轴标注（X/Y/Z），禁用标注文字自动缩放以保证小尺寸下清晰。
 * @return 坐标系 vtkAxesActor（已按 kAxesLengthMeter 设定长度）。
 */
static vtkSmartPointer<vtkAxesActor> CreateOriginAxes()
{
    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(kAxesLengthMeter, kAxesLengthMeter, kAxesLengthMeter);
    axes->SetShaftTypeToCylinder();

    vtkCaptionActor2D* captions[3] = {
        axes->GetXAxisCaptionActor2D(),
        axes->GetYAxisCaptionActor2D(),
        axes->GetZAxisCaptionActor2D(),
    };
    for (vtkCaptionActor2D* caption : captions)
    {
        caption->GetTextActor()->SetTextScaleModeToNone();   // 禁用自动缩放
        caption->GetCaptionTextProperty()->SetFontSize(14);
        caption->GetCaptionTextProperty()->BoldOff();
        caption->GetCaptionTextProperty()->ItalicOff();
        caption->GetCaptionTextProperty()->ShadowOff();
    }
    return axes;
}

/**
 * @brief 为文字属性挑选系统里的中文字体。
 * @details VTK 默认字体不含中文字形，直接显示中文会变成方块。这里按优先级尝试
 *          系统常见中文字体，找到第一个存在的就设给文本属性；都没找到时保持默认字体
 *          （此时中文可能显示为方块，但不影响程序运行）。
 * @param textProperty [in] 待设置字体的文字属性。
 */
static void ApplyChineseFont(vtkTextProperty* textProperty)
{
#ifdef _WIN32
    // 注意：VTK 的 FreeType 封装对 .ttc 字体集合加载不可靠（中文会渲染不出来），
    // 优先使用单体 .ttf。
    const char* candidates[] = {
        "C:/Windows/Fonts/simhei.ttf",   // 黑体（单体 TTF，最稳）
        "C:/Windows/Fonts/Deng.ttf",     // 等线
        "C:/Windows/Fonts/NotoSansSC-VF.ttf",
        "C:/Windows/Fonts/msyh.ttc",     // 微软雅黑（集合字体，仅作兜底）
    };
#else
    const char* candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
#endif

    for (const char* path : candidates)
    {
        std::ifstream fontFile(path, std::ios::binary);
        if (fontFile.good())
        {
            // FontFile 仅在 FontFamily == VTK_FONT_FILE 时生效（见 vtkTextProperty 文档）。
            textProperty->SetFontFamily(VTK_FONT_FILE);
            textProperty->SetFontFile(path);
            return;
        }
    }
}

/**
 * @brief 骨骼格式枚举 → 左上角状态栏展示用的名称。
 * @param format [in] 当前缓存帧的骨骼格式。
 * @return 中文格式说明；非法值返回"未知"。
 */
static const char* SkeletonFormatDisplayName(GloveStreamSkeletonFormat_TypeDef format)
{
    switch (format)
    {
    case GloveStreamFormat_Skeleton42:
        return "42 段（含指尖末端）";
    case GloveStreamFormat_Skeleton32:
        return "32 段（指尖按第三节姿态生成）";
    case GloveStreamFormat_OpenXR:
        return "OpenXR（暂不支持）";
    default:
        return "未知";
    }
}

/**
 * @brief 创建窗口左上角的状态叠加文字（端口状态 / 骨骼模式 / 帧号）。
 * @details 用 vtkCornerAnnotation 的左上角槽位（corner index = 2），窗口缩放时位置自适应。
 * @return 已设置好字体与颜色的叠加文字对象。
 */
static vtkSmartPointer<vtkCornerAnnotation> CreateStatusOverlay()
{
    auto annotation = vtkSmartPointer<vtkCornerAnnotation>::New();
    annotation->SetText(2, "");   // 2 = 左上角
    annotation->GetTextProperty()->SetColor(0.85, 0.92, 1.00);
    annotation->GetTextProperty()->SetFontSize(18);
    annotation->GetTextProperty()->ShadowOn();
    ApplyChineseFont(annotation->GetTextProperty());
    return annotation;
}

/**
 * @brief 判断一帧数据的全部骨骼位置是否均为零（未收到有效位置的判定依据）。
 * @param skeletons [in] 骨骼数组。
 * @param count [in] 骨骼段数。
 * @return true 表示全部位置接近零；false 表示至少一段位置有效。
 */
static bool FramePositionsAllZero(const SingleSkeletonDef* skeletons, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const float* p = skeletons[i].position_meter;
        if (std::fabs(p[0]) > 1e-6f || std::fabs(p[1]) > 1e-6f || std::fabs(p[2]) > 1e-6f)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief 一组要画成小球的骨骼点：槽位表 + 该组独立的点容器。
 * @details vtkGlyph3DMapper 会在输入数据集的**所有点**上放小球（不理会顶点单元），
 *          因此每组必须有自己的一份点容器，否则后创建的组会覆盖先创建的组的颜色。
 */
struct JointGroup
{
    const int* slots;                        ///< 统一布局槽位表。
    int count;                               ///< 该组槽位个数。
    vtkSmartPointer<vtkPoints> points;       ///< 该组的点容器，顺序与 slots 一致。
};

/**
 * @brief 整个骨架的几何数据：42 个槽位的当前坐标 + 连线用的点容器 + 2 组骨骼点容器。
 * @details 除点容器外还持有 4 个 vtkPolyData 指针：每帧改完坐标后必须对它们调
 *          Modified()。原因：vtkPoints::SetPoint() 只会更新 vtkPoints 自己的
 *          MTime，而 mapper 判断"是否需要重建绘制数据"看的是输入 vtkPolyData 的
 *          MTime；不显式 Modified() 的话 VTK 会跳过更新，表现为
 *          "数据明明在变、3D 画面却完全不动"。
 */
struct SkeletonGeometry
{
    std::array<std::array<double, 3>, kJointCount> current{};   ///< 各槽位当前坐标，单位米。
    vtkSmartPointer<vtkPoints> bonePoints;                      ///< 连线用的 42 个点。
    JointGroup groups[2];                                       ///< 右手 / 左手（各 21 个槽位，含指尖末端）。
    vtkSmartPointer<vtkPolyData> groupPolyData[2];              ///< 每组的顶点 polydata（球 glyph 输入）。
    vtkSmartPointer<vtkPolyData> linePolyData[2];               ///< 左右手骨骼连线 polydata。
};

/**
 * @brief 把 SkeletonGeometry::current 的坐标同步到各个 vtkPoints，并标记 polydata 已修改。
 * @param geometry [in] 待同步的骨架几何数据。
 */
static void SyncGeometryPoints(SkeletonGeometry& geometry)
{
    for (int g = 0; g < 2; ++g)
    {
        JointGroup& group = geometry.groups[g];
        for (int k = 0; k < group.count; ++k)
        {
            const auto& p = geometry.current[group.slots[k]];
            group.points->SetPoint(k, p[0], p[1], p[2]);
        }
        group.points->Modified();
        if (geometry.groupPolyData[g].GetPointer() != nullptr)
        {
            geometry.groupPolyData[g]->Modified();
        }
    }

    for (int i = 0; i < kJointCount; ++i)
    {
        const auto& p = geometry.current[i];
        geometry.bonePoints->SetPoint(i, p[0], p[1], p[2]);
    }
    geometry.bonePoints->Modified();
    for (int g = 0; g < 2; ++g)
    {
        if (geometry.linePolyData[g].GetPointer() != nullptr)
        {
            geometry.linePolyData[g]->Modified();
        }
    }
}

/**
 * @brief 用一组骨骼点创建只含顶点单元的 vtkPolyData，供 glyph 画球。
 * @param points [in] 该组自己的点容器。
 * @param count [in] 点个数。
 * @return 含 count 个顶点单元的 vtkPolyData。
 */
static vtkSmartPointer<vtkPolyData> MakeJointVertices(vtkPoints* points, int count)
{
    auto verts = vtkSmartPointer<vtkCellArray>::New();
    for (int i = 0; i < count; ++i)
    {
        const vtkIdType id = static_cast<vtkIdType>(i);
        verts->InsertNextCell(1, &id);
    }

    auto polydata = vtkSmartPointer<vtkPolyData>::New();
    polydata->SetPoints(points);
    polydata->SetVerts(verts);
    return polydata;
}

/**
 * @brief 用骨骼点容器与骨骼连线表生成骨架线 vtkPolyData。
 * @param points [in] 连线用的 42 个点。
 * @param firstEdge [in] 起始连线在 kBoneEdges 中的下标（右手 0~19，左手 20~39）。
 * @param edgeCount [in] 本次生成的连线数量。
 * @return 含 edgeCount 条骨骼连线的 vtkPolyData。
 */
static vtkSmartPointer<vtkPolyData> BuildBoneLines(vtkPoints* points, int firstEdge, int edgeCount)
{
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (int i = firstEdge; i < firstEdge + edgeCount; ++i)
    {
        const vtkIdType ids[2] = { kBoneEdges[i][0], kBoneEdges[i][1] };
        lines->InsertNextCell(2, ids);
    }

    auto polydata = vtkSmartPointer<vtkPolyData>::New();
    polydata->SetPoints(points);
    polydata->SetLines(lines);
    return polydata;
}

/**
 * @brief 用 vtkGlyph3DMapper 在 polydata 的每个顶点上画一个球。
 * @param joints [in] 只含顶点单元的 vtkPolyData（骨骼点）。
 * @param spherePort [in] 球体 source 的输出端口，决定球半径与细分度。
 * @param rgb [in] 球的颜色，分量范围 0~1。
 * @return 小球 actor。
 */
static vtkSmartPointer<vtkActor> CreateJointGlyphActor(vtkPolyData* joints,
                                                       vtkAlgorithmOutput* spherePort,
                                                       const double rgb[3])
{
    auto mapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
    mapper->SetInputData(joints);
    mapper->SetSourceConnection(spherePort);
    mapper->ScalingOff();          // 球半径由 source 决定，不随数据范围缩放
    mapper->SetScalarVisibility(0);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
    return actor;
}

/**
 * @brief VTK 定时器回调：每帧从 SDK 取最新骨骼帧并刷新 42 个骨骼点。
 * @details 挂接在 vtkRenderWindowInteractor 的 TimerEvent 上。
 *          依据帧头 skeletonFormat 选择 42 段或 32 段取帧接口，再统一映射到 42 点布局：
 *          42 段直接用自带的指尖末端点；32 段则按最后一节方向外推一个末节。
 */
class VTKHandTimerCallback final : public vtkCommand
{
public:
    /** @brief VTK 标准工厂方法：创建回调实例（等价于 new + Delete 计数管理）。 @return 新实例指针。 */
    static VTKHandTimerCallback* New() { return new VTKHandTimerCallback(); }

    /** @brief 设置用于触发重绘的渲染窗口。 @param rw [in] 目标渲染窗口指针。 */
    void SetRenderWindow(vtkRenderWindow* rw) { this->RenderWindow = rw; }

    /** @brief 设置骨架几何数据（坐标缓存 + 各组点容器）。 @param geometry [in] 骨架几何数据指针。 */
    void SetGeometry(SkeletonGeometry* geometry) { this->Geometry = geometry; }

    /** @brief 设置未收到有效位置时使用的默认布局坐标。 @param fallback [in] 默认坐标数组指针。 */
    void SetFallbackPositions(std::array<std::array<double, 3>, kJointCount>* fallback)
    {
        this->FallbackPositions = fallback;
    }

    /** @brief 设置窗口左上角的状态叠加文字。 @param overlay [in] 叠加文字对象指针。 */
    void SetStatusOverlay(vtkCornerAnnotation* overlay) { this->StatusOverlay = overlay; }

    /**
     * @brief 开启原始 32 段帧导出（诊断用）。
     * @param file [in] 已打开的文本文件。
     * @param frames [in] 最多导出多少帧。
     */
    void SetDumpFile(std::FILE* file, int frames)
    {
        this->DumpFile = file;
        this->DumpRemaining = frames;
    }

    /**
     * @brief 记录 UDP 端口绑定结果，供状态栏显示。
     * @param port [in] 监听端口号。
     * @param opened [in] true 表示绑定成功（正在监听）；false 表示被占用。
     * @param errorCode [in] 绑定失败时的 SDK 返回码。
     */
    void SetPortStatus(int port, bool opened, int errorCode)
    {
        this->Port = port;
        this->PortOpened = opened;
        this->PortErrorCode = errorCode;
    }

    /**
     * @brief VTK 事件处理：TimerEvent 到来时从 SDK 取最新帧并更新骨骼点坐标。
     * @param caller [in] 事件来源对象（未使用）。
     * @param eventId [in] 事件编号（仅处理 TimerEvent）。
     * @param callData [in] 事件附加数据（未使用）。
     */
    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override
    {
        (void)caller;
        (void)callData;

        if (eventId != vtkCommand::TimerEvent || this->Geometry == nullptr)
        {
            return;
        }

        ++this->TickCount;

        if (MotionGloveSDK_isGloveNewFramePending(kDefaultGloveName))
        {
            ++this->PendingCount;

            GloveStreamSkeletonFormat_TypeDef format = GloveStreamFormat_Skeleton32;
            if (MotionGloveSDK_GetSkeletonsFrameType(kDefaultGloveName, &format) == 0)
            {
                // 主机端可能动态切换 32/42 段：每帧都按帧头重新取格式，状态栏同帧刷新。
                this->CurrentFormat = format;

                bool updated = false;
                if (format == GloveStreamFormat_Skeleton42)
                {
                    updated = UpdateFromFrame42();
                }
                else if (format == GloveStreamFormat_Skeleton32)
                {
                    updated = UpdateFromFrame32();
                }
                else
                {
                    ReportFormatOnce("unsupported", "不支持的骨骼格式，已跳过该帧");
                }

                if (updated)
                {
                    SyncGeometryPoints(*this->Geometry);
                }
                else
                {
                    ++this->FrameFailCount;
                }
            }
            else
            {
                ++this->FrameFailCount;
            }
            MotionGloveSDK_resetGloveNewFramePending(kDefaultGloveName);
        }

        PrintDiagOnce();
        UpdateStatusText();

        // Timer callback pattern: let VTK own the event loop; we simply update state and request a render.
        if (this->RenderWindow != nullptr)
        {
            this->RenderWindow->Render();
        }
    }

private:
    /** @brief 每 2 秒输出一次取帧状态诊断日志，便于定位"骨骼不动"类问题。 */
    void PrintDiagOnce()
    {
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->LastDiagTime).count() < 2000)
        {
            return;
        }
        this->LastDiagTime = now;

        // 手掌根（槽位 0 / 21）与指尖（槽位 20 / 41）的当前坐标，
        // 直接反映真实帧数据是否到达、左右手是否在动。
        const auto& p0 = this->Geometry->current[0];
        const auto& p21 = this->Geometry->current[kLeftHandBaseSlot];
        const auto& p20 = this->Geometry->current[20];
        const auto& p41 = this->Geometry->current[41];
        printf("[VTK diag] ticks=%llu pending=%llu f42=%llu f32=%llu fail=%llu allZero=%llu fn=%u\n"
               "           pR0=(%.4f, %.4f, %.4f) pRtip20=(%.4f, %.4f, %.4f)\n"
               "           pL21=(%.4f, %.4f, %.4f) pLtip41=(%.4f, %.4f, %.4f)\n",
               this->TickCount, this->PendingCount, this->Frame42Count, this->Frame32Count,
               this->FrameFailCount, this->AllZeroFrameCount, this->LastFrameNumber,
               p0[0], p0[1], p0[2], p20[0], p20[1], p20[2],
               p21[0], p21[1], p21[2], p41[0], p41[1], p41[2]);

        // 末节（P3→指尖）与上一节（P2→P3）的夹角：0° 说明指尖退回了"沿上一节直接延长"。
        auto bendDeg = [&](int p2, int p3, int tip) -> double
            {
                double v1[3] = { this->Geometry->current[p3][0] - this->Geometry->current[p2][0],
                                 this->Geometry->current[p3][1] - this->Geometry->current[p2][1],
                                 this->Geometry->current[p3][2] - this->Geometry->current[p2][2] };
                double v2[3] = { this->Geometry->current[tip][0] - this->Geometry->current[p3][0],
                                 this->Geometry->current[tip][1] - this->Geometry->current[p3][1],
                                 this->Geometry->current[tip][2] - this->Geometry->current[p3][2] };
                if (!NormalizeVector(v1) || !NormalizeVector(v2))
                {
                    return -1.0;
                }
                double dot = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
                if (dot > 1.0) dot = 1.0;
                if (dot < -1.0) dot = -1.0;
                return std::acos(dot) * 180.0 / 3.14159265358979323846;
            };
        printf("           bend: R小指=%.1f° R食指=%.1f° L小指=%.1f° L食指=%.1f°\n",
               bendDeg(18, 19, 20), bendDeg(6, 7, 8), bendDeg(39, 40, 41), bendDeg(27, 28, 29));

        // 帧头的姿态设置：确认四元数是"相对父骨骼"（relative）还是全局（global）。
        printf("           coord=%s att=%s\n",
               this->LastCoordinate == SkeletonCoordinate_Global ? "global" : "relative",
               this->LastAttitude == SkeletonAttitude_Quat ? "quat" :
               (this->LastAttitude == SkeletonAttitude_Euler ? "euler" : "none"));
    }

    /**
     * @brief 刷新窗口左上角状态栏：端口状态 / 骨骼模式 / 帧号。
     * @details 每帧调用；只有文本内容变化时才真正 SetText，避免每帧重建文字纹理。
     *          骨骼模式与帧号都来自最新缓存帧，主机端动态切换 32/42 段时立即跟随。
     */
    void UpdateStatusText()
    {
        if (this->StatusOverlay == nullptr)
        {
            return;
        }

        const char* portState = this->PortOpened ? "监听中" : "被占用";
        const char* formatName = (this->CurrentFormat == GloveStreamFormat_Count)
                                     ? "等待数据…"
                                     : SkeletonFormatDisplayName(this->CurrentFormat);

        char frameText[32];
        if (this->HasFrame)
        {
            std::snprintf(frameText, sizeof(frameText), "%u", this->LastFrameNumber);
        }
        else
        {
            std::snprintf(frameText, sizeof(frameText), "-");
        }

        char buffer[512];
        if (this->PortOpened)
        {
            std::snprintf(buffer, sizeof(buffer),
                          "UDP 0.0.0.0:%d（所有网卡）  %s\n骨骼模式：%s\n帧号：%s",
                          this->Port, portState, formatName, frameText);
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer),
                          "UDP 0.0.0.0:%d（所有网卡）  %s（code=%d）\n骨骼模式：%s\n帧号：%s",
                          this->Port, portState, this->PortErrorCode, formatName, frameText);
        }

        if (this->StatusCache == buffer)
        {
            return;
        }
        this->StatusCache = buffer;

        this->StatusOverlay->SetText(2, buffer);
        this->StatusOverlay->GetTextProperty()->SetColor(
            this->PortOpened ? 0.85 : 1.00,
            this->PortOpened ? 0.92 : 0.35,
            this->PortOpened ? 1.00 : 0.35);
    }

    /**
     * @brief 写入一个骨骼点：位置全零时用默认布局，同时记录到坐标缓存。
     * @param slot [in] 关节槽位（0-41）。
     * @param positionMeter [in] 关节全局位置（米，xyz）。
     * @param allZeroPositions [in] 本帧全部位置为零时为 true，用静态默认布局兜底。
     */
    void ApplyJointPosition(int slot, const float positionMeter[3], bool allZeroPositions)
    {
        double x = static_cast<double>(positionMeter[0]);
        double y = static_cast<double>(positionMeter[1]);
        double z = static_cast<double>(positionMeter[2]);

        if (allZeroPositions && this->FallbackPositions != nullptr)
        {
            x = (*this->FallbackPositions)[slot][0];
            y = (*this->FallbackPositions)[slot][1];
            z = (*this->FallbackPositions)[slot][2];
        }

        this->Geometry->current[slot][0] = x;
        this->Geometry->current[slot][1] = y;
        this->Geometry->current[slot][2] = z;
    }

    /**
     * @brief 42 段帧：每段（含 10 个指尖末端）直接对应统一布局的同名槽位。
     * @return true 表示成功取到帧并更新了坐标。
     */
    bool UpdateFromFrame42()
    {
        KHHS42PosAttitude_TypeDef frame;
        if (MotionGloveSDK_GetSkeletons42Frame(kDefaultGloveName, &frame) != 0)
        {
            return false;
        }

        ++this->Frame42Count;
        this->LastFrameNumber = frame.hdr.frameNumber;
        this->HasFrame = true;

        const bool allZeroPos = FramePositionsAllZero(frame.skeletons, KHHS42_SkeletonCount);
        if (allZeroPos)
        {
            ++this->AllZeroFrameCount;
        }
        for (int i = 0; i < kJointCount; ++i)
        {
            ApplyJointPosition(i, frame.skeletons[i].position_meter, allZeroPos);
        }

        ReportFormatOnce("42",
            "42 段骨骼：10 个指尖末端点直接取帧数据");
        return true;
    }

    /**
     * @brief 32 段帧：映射 32 个关节槽位，再按各指第三段姿态生成 10 个虚拟指尖末端。
     * @return true 表示成功取到帧并更新了坐标。
     */
    bool UpdateFromFrame32()
    {
        KHHS32PosAttitude_TypeDef frame;
        if (MotionGloveSDK_GetSkeletons32Frame(kDefaultGloveName, &frame) != 0)
        {
            return false;
        }

        ++this->Frame32Count;
        this->LastFrameNumber = frame.hdr.frameNumber;
        this->HasFrame = true;

        const bool allZeroPos = FramePositionsAllZero(frame.skeletons, KHHS32_SkeletonCount);
        if (allZeroPos)
        {
            ++this->AllZeroFrameCount;
        }
        for (int i = 0; i < KHHS32_SkeletonCount; ++i)
        {
            ApplyJointPosition(kSlotOfSkeleton32[i], frame.skeletons[i].position_meter, allZeroPos);
        }

        // 32 段没有指尖末端点：末节的朝向取该指第三段骨骼的姿态——
        // 把骨骼静止轴向用第三段的全局四元数旋转后延伸固定长度（不是沿上一节直接延长）。
        // 位置全零走默认布局时不重算指尖，直接沿用默认布局里的指尖点。
        if (!allZeroPos)
        {
            std::array<std::array<double, 4>, kJointCount> globalQuats{};
            std::array<bool, kJointCount> quatValid{};
            BuildGlobalQuaternions(frame, globalQuats, quatValid);

            // 记录帧头的姿态/坐标系设置，供诊断日志显示。
            this->LastCoordinate = frame.hdr.skeletonCoordinate;
            this->LastAttitude = frame.hdr.skeletonAttitude;

            for (int t = 0; t < kTipSlotCount; ++t)
            {
                const int tipSlot = kTipSlots[t];
                const int thirdSlot = tipSlot - 1;
                const int secondSlot = tipSlot - 2;
                double tip[3] = { 0.0, 0.0, 0.0 };
                ComputeVirtualTipPosition(this->Geometry->current[secondSlot].data(),
                                          this->Geometry->current[thirdSlot].data(),
                                          globalQuats[thirdSlot].data(), quatValid[thirdSlot],
                                          tip);

                this->Geometry->current[tipSlot][0] = tip[0];
                this->Geometry->current[tipSlot][1] = tip[1];
                this->Geometry->current[tipSlot][2] = tip[2];
            }
        }

        ReportFormatOnce("32",
            "32 段骨骼：无指尖末端点，已按第三段骨骼姿态定向并延伸 20mm 生成虚拟指尖");

        if (this->DumpFile != nullptr && this->DumpRemaining > 0)
        {
            DumpFrame32(frame);
        }
        return true;
    }

    /**
     * @brief 把一帧 32 段原始数据写成一行 CSV（诊断用）：fn + 32 x (pos xyz + quat xyzw)。
     * @param frame [in] 32 段骨骼帧。
     */
    void DumpFrame32(const KHHS32PosAttitude_TypeDef& frame)
    {
        fprintf(this->DumpFile, "%u", frame.hdr.frameNumber);
        for (int i = 0; i < KHHS32_SkeletonCount; ++i)
        {
            const SingleSkeletonDef& s = frame.skeletons[i];
            fprintf(this->DumpFile, ",%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",
                    s.position_meter[0], s.position_meter[1], s.position_meter[2],
                    s.quat_xyzw[0], s.quat_xyzw[1], s.quat_xyzw[2], s.quat_xyzw[3]);
        }
        fprintf(this->DumpFile, "\n");
        --this->DumpRemaining;
        if (this->DumpRemaining == 0)
        {
            fflush(this->DumpFile);
        }
    }

    /**
     * @brief 骨骼格式变化时打印一次说明，避免每帧刷屏。
     * @param formatTag [in] 当前格式标识（变化检测用）。
     * @param message [in] 要打印的说明文字。
     */
    void ReportFormatOnce(const char* formatTag, const char* message)
    {
        if (this->LastFormatTag == formatTag)
        {
            return;
        }
        this->LastFormatTag = formatTag;
        printf("[VTK] %s\n", message);
    }

    /** @brief 触发重绘的渲染窗口（nullptr 表示未设置，如 headless 模式）。 */
    vtkRenderWindow* RenderWindow = nullptr;
    /** @brief 骨架几何数据（坐标缓存 + 各组点容器）。 */
    SkeletonGeometry* Geometry = nullptr;
    /** @brief 默认布局坐标指针，位置全零时使用。 */
    std::array<std::array<double, 3>, kJointCount>* FallbackPositions = nullptr;
    /** @brief 窗口左上角的状态叠加文字（nullptr 表示未设置）。 */
    vtkCornerAnnotation* StatusOverlay = nullptr;
    /** @brief 监听的 UDP 端口号（状态栏展示用）。 */
    int Port = 0;
    /** @brief 端口是否绑定成功（true 监听中 / false 被占用）。 */
    bool PortOpened = true;
    /** @brief 端口绑定失败时的 SDK 返回码。 */
    int PortErrorCode = 0;
    /** @brief 当前缓存帧的骨骼格式；GloveStreamFormat_Count 表示尚未收到任何帧。 */
    GloveStreamSkeletonFormat_TypeDef CurrentFormat = GloveStreamFormat_Count;
    /** @brief 是否已成功收到过至少一帧（决定帧号显示 "-" 还是真实值）。 */
    bool HasFrame = false;
    /** @brief 上一次写入状态栏的文本，用于避免每帧重复更新。 */
    std::string StatusCache;
    /** @brief 上一次打印过的骨骼格式标签（空表示尚未收到任何帧）。 */
    std::string LastFormatTag;

    /** @brief 定时器触发总次数。 */
    unsigned long long TickCount = 0;
    /** @brief 检测到"有新帧"的次数。 */
    unsigned long long PendingCount = 0;
    /** @brief 成功取到 42 段帧的次数。 */
    unsigned long long Frame42Count = 0;
    /** @brief 成功取到 32 段帧的次数。 */
    unsigned long long Frame32Count = 0;
    /** @brief 取帧失败（格式不符 / 接口返回非 0）的次数。 */
    unsigned long long FrameFailCount = 0;
    /** @brief 收到整帧但坐标全零（使用了默认布局）的次数。 */
    unsigned long long AllZeroFrameCount = 0;
    /** @brief 最近一帧的帧序号。 */
    unsigned int LastFrameNumber = 0;
    /** @brief 最近一帧的骨骼旋转系设置（诊断用）。 */
    SkeletonCoordinate_TypeDef LastCoordinate = SkeletonCoordinate_Relative;
    /** @brief 最近一帧的姿态格式设置（诊断用）。 */
    SkeletonAttitude_TypeDef LastAttitude = SkeletonAttitude_None;
    /** @brief 原始帧导出文件（nullptr 表示不导出，诊断用）。 */
    std::FILE* DumpFile = nullptr;
    /** @brief 还能导出多少帧（0 表示已结束）。 */
    int DumpRemaining = 0;    /** @brief 上一次诊断日志的打印时刻。 */
    std::chrono::steady_clock::time_point LastDiagTime = std::chrono::steady_clock::now();
};

/** @brief UDP 端口是否已关闭的标志，保证关闭操作只执行一次。 */
static bool gUdpClosed = false;

/** @brief 幂等关闭 UDP 端口：仅在首次调用时执行 CloseUDPPort。 */
static void CloseUdpPortOnce()
{
    if (!gUdpClosed)
    {
        MotionGloveSDK_CloseUDPPort();
        gUdpClosed = true;
    }
}

/**
 * @brief VTK 退出事件回调：退出交互循环时关闭 UDP 端口。
 * @param caller [in] 事件来源对象（未使用）。
 * @param eventId [in] 事件编号（未使用）。
 * @param clientData [in] 用户数据（未使用）。
 * @param callData [in] 事件附加数据（未使用）。
 */
static void OnExit(vtkObject* caller, unsigned long eventId, void* clientData, void* callData)
{
    (void)caller;
    (void)eventId;
    (void)clientData;
    (void)callData;
    CloseUdpPortOnce();
}

/**
 * @brief VTK 按键回调：按下 q/Q 时关闭 UDP 端口并结束渲染窗口。
 * @param caller [in] 事件来源对象（vtkRenderWindowInteractor）。
 * @param eventId [in] 事件编号（未使用）。
 * @param clientData [in] 用户数据（未使用）。
 * @param callData [in] 事件附加数据（未使用）。
 */
static void OnKeyPress(vtkObject* caller, unsigned long eventId, void* clientData, void* callData)
{
    (void)eventId;
    (void)clientData;
    (void)callData;

    auto* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!interactor)
    {
        return;
    }

    const std::string key = interactor->GetKeySym() ? interactor->GetKeySym() : "";
    if (key == "q" || key == "Q")
    {
        CloseUdpPortOnce();
        interactor->GetRenderWindow()->Finalize();
        interactor->TerminateApp();
    }
}

/**
 * @brief 程序入口：绑定 UDP 端口后启动 VTK 3D 窗口，实时渲染手部骨架。
 * @details 支持 `--self-test`：只走完初始化路径（UDP 绑定 + VTK 场景搭建）就退出，
 *          不创建 OpenGL 上下文、不进事件循环，便于在无显卡的 CI 环境做启动自检。
 * @param argc [in] 命令行参数个数。
 * @param argv [in] 命令行参数数组。
 * @return 0 表示正常退出；1 表示 UDP 绑定失败；
 *         2 表示当前环境不支持 OpenGL 3.2+（VTK 无法渲染）。
 */
int main(int argc, char* argv[])
{
    bool selfTest = false;
    double headlessSeconds = -1.0;   // <0 表示不启用 headless 模式
    double renderCheckSeconds = -1.0;   // <0 表示不启用离屏渲染自检
    const char* renderOutDir = ".";
    const char* dumpPath = nullptr;      // --dump：导出原始 32 段帧（诊断用）
    int dumpFrames = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--self-test") == 0)
        {
            selfTest = true;
        }
        else if (std::strcmp(argv[i], "--headless") == 0 && i + 1 < argc)
        {
            headlessSeconds = std::atof(argv[i + 1]);
            if (headlessSeconds <= 0.0)
            {
                headlessSeconds = 3.0;
            }
        }
        else if (std::strcmp(argv[i], "--render-check") == 0 && i + 1 < argc)
        {
            renderCheckSeconds = std::atof(argv[i + 1]);
            if (renderCheckSeconds <= 0.0)
            {
                renderCheckSeconds = 4.0;
            }
        }
        else if (std::strcmp(argv[i], "--render-out") == 0 && i + 1 < argc)
        {
            renderOutDir = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
        {
            dumpPath = argv[i + 1];
        }
        else if (std::strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc)
        {
            dumpFrames = std::atoi(argv[i + 1]);
        }
    }
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    printf("motionGloveSDK_example_vtk v%s: binding UDP 0.0.0.0:%d (all interfaces)\n",
           MOTIONGLOVE_SDK_VER_STRING, kDefaultUdpPort);
    const int listenRet = MotionGloveSDK_ListenUDPPort(kDefaultUdpPort);
    if (listenRet != 0)
    {
        // 端口被占用时不直接退出：照样打开窗口，并在左上角状态栏标出"被占用"及错误码，
        // 让用户一眼看到原因（多见于上一次实例没关干净，或上位机已占用该端口）。
        printf("Failed to bind UDP port %d (code=%d). Is the port already in use?\n", kDefaultUdpPort, listenRet);
        printf("窗口仍会打开：左上角会显示“端口被占用”，请先关闭占用该端口的程序。\n");
    }

    // VTK window should appear immediately, even before any glove frame is received.
    // 背景色与 Python 版一致 (0.10, 0.10, 0.16)。
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.10, 0.10, 0.16);

    // ── 地平面网格：XZ 平面（y = 0），参数与 Python 版一致 ────────────────
    auto gridMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    gridMapper->SetInputData(BuildGroundGrid());

    auto gridActor = vtkSmartPointer<vtkActor>::New();
    gridActor->SetMapper(gridMapper);
    gridActor->GetProperty()->SetColor(0.40, 0.40, 0.40);
    gridActor->GetProperty()->SetLineWidth(1.0f);
    renderer->AddActor(gridActor);

    // ── 场景中间（原点）的坐标系：vtkAxesActor，带 X/Y/Z 轴标注 ───────────
    renderer->AddActor(CreateOriginAxes());

    // ── 左上角状态叠加：端口状态 / 骨骼模式 / 帧号（每帧刷新）───────────────
    auto statusOverlay = CreateStatusOverlay();
    renderer->AddActor(statusOverlay);   // vtkCornerAnnotation 是 vtkActor2D 子类，同样走 AddActor

    auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("MotionGloveSDK VTK Example");
    renderWindow->SetSize(1000, 800);

    auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    interactor->SetInteractorStyle(style);

    // ── 骨架：42 个骨骼点（小球）+ 40 条连线（手型）────────────────────────
    auto fallbackPositions = BuildFallbackPositions();

    SkeletonGeometry geometry;
    geometry.current = fallbackPositions;

    // 2 组骨骼点（右手 / 左手，各 21 个槽位）各自独立一份 vtkPoints：
    // vtkGlyph3DMapper 会在输入的所有点上放小球，共用一份点容器
    // 会让后建组覆盖先建组的颜色。
    static const int kRightAllSlots[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    };
    static const int kLeftAllSlots[] = {
        21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    };

    struct GroupSpec
    {
        const int* slots;
        int count;
    };
    const GroupSpec groupSpecs[2] = {
        { kRightAllSlots, static_cast<int>(sizeof(kRightAllSlots) / sizeof(kRightAllSlots[0])) },
        { kLeftAllSlots, static_cast<int>(sizeof(kLeftAllSlots) / sizeof(kLeftAllSlots[0])) },
    };

    for (int g = 0; g < 2; ++g)
    {
        geometry.groups[g].slots = groupSpecs[g].slots;
        geometry.groups[g].count = groupSpecs[g].count;
        geometry.groups[g].points = vtkSmartPointer<vtkPoints>::New();
        geometry.groups[g].points->SetNumberOfPoints(groupSpecs[g].count);
    }

    geometry.bonePoints = vtkSmartPointer<vtkPoints>::New();
    geometry.bonePoints->SetNumberOfPoints(kJointCount);

    // 先把默认布局写入各个点容器，窗口一打开就能看到手型骨架。
    SyncGeometryPoints(geometry);

    // ── 骨骼点小球：左右手分别着色（与 Python 版一致）──────────────────────
    // 右手青色 (0.3, 0.8, 1.0)，左手橙色 (1.0, 0.5, 0.2)。
    auto jointSphere = vtkSmartPointer<vtkSphereSource>::New();
    jointSphere->SetRadius(kJointRadiusMeter);
    jointSphere->SetThetaResolution(16);
    jointSphere->SetPhiResolution(16);

    const double colorRight[3] = { 0.30, 0.80, 1.00 };
    const double colorLeft[3] = { 1.00, 0.50, 0.20 };
    const double* groupColors[2] = { colorRight, colorLeft };

    for (int g = 0; g < 2; ++g)
    {
        auto jointPolyData = MakeJointVertices(geometry.groups[g].points, geometry.groups[g].count);
        geometry.groupPolyData[g] = jointPolyData;   // 供每帧 Modified() 用
        renderer->AddActor(CreateJointGlyphActor(jointPolyData, jointSphere->GetOutputPort(),
                                                 groupColors[g]));
    }

    // ── 骨骼连线：小球之间的连线，颜色随左右手（与 Python 版一致）─────────
    // kBoneEdges 前 20 条为右手，后 20 条为左手；线宽 3.0。
    const int kHalfEdgeCount = kBoneEdgeCount / 2;

    auto rightLinePolyData = BuildBoneLines(geometry.bonePoints, 0, kHalfEdgeCount);
    geometry.linePolyData[0] = rightLinePolyData;   // 供每帧 Modified() 用
    auto rightLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    rightLineMapper->SetInputData(rightLinePolyData);

    auto rightLineActor = vtkSmartPointer<vtkActor>::New();
    rightLineActor->SetMapper(rightLineMapper);
    rightLineActor->GetProperty()->SetColor(colorRight[0], colorRight[1], colorRight[2]);
    rightLineActor->GetProperty()->SetLineWidth(3.0f);
    renderer->AddActor(rightLineActor);

    auto leftLinePolyData = BuildBoneLines(geometry.bonePoints, kHalfEdgeCount,
                                           kBoneEdgeCount - kHalfEdgeCount);
    geometry.linePolyData[1] = leftLinePolyData;   // 供每帧 Modified() 用

    auto leftLineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    leftLineMapper->SetInputData(leftLinePolyData);

    auto leftLineActor = vtkSmartPointer<vtkActor>::New();
    leftLineActor->SetMapper(leftLineMapper);
    leftLineActor->GetProperty()->SetColor(colorLeft[0], colorLeft[1], colorLeft[2]);
    leftLineActor->GetProperty()->SetLineWidth(3.0f);
    renderer->AddActor(leftLineActor);

    // ── 相机初始视角：与 Python 版 camera_control.setup_camera 完全一致 ───
    auto camera = renderer->GetActiveCamera();
    camera->SetPosition(-0.0011, 0.5294, 0.7502);
    camera->SetFocalPoint(-0.0117, 0.0795, 0.0005);
    camera->SetViewUp(-0.0033, 0.8574, -0.5146);
    renderer->ResetCameraClippingRange();

    // Exit handlers: close UDP port on window close or 'Q'.
    auto exitCb = vtkSmartPointer<vtkCallbackCommand>::New();
    exitCb->SetCallback(OnExit);

    auto keyCb = vtkSmartPointer<vtkCallbackCommand>::New();
    keyCb->SetCallback(OnKeyPress);

    // Closing the render window makes the interactor fire ExitEvent, so a single
    // observer on the interactor covers both window close and the 'Q' key.
    interactor->AddObserver(vtkCommand::ExitEvent, exitCb);
    interactor->AddObserver(vtkCommand::KeyPressEvent, keyCb);

    // Timer callback pattern: poll SDK at ~60 Hz.
    auto timerCb = vtkSmartPointer<VTKHandTimerCallback>::New();
    timerCb->SetRenderWindow(renderWindow);
    timerCb->SetGeometry(&geometry);
    timerCb->SetFallbackPositions(&fallbackPositions);
    timerCb->SetStatusOverlay(statusOverlay);
    timerCb->SetPortStatus(kDefaultUdpPort, listenRet == 0, listenRet);

    // 诊断：可选导出原始 32 段帧，便于离线核对姿态与位置的参考系是否一致。
    if (dumpPath != nullptr)
    {
        std::FILE* dumpFile = std::fopen(dumpPath, "w");
        if (dumpFile != nullptr)
        {
            timerCb->SetDumpFile(dumpFile, dumpFrames > 0 ? dumpFrames : 300);
            printf("dumping raw skeleton_32 frames to %s\n", dumpPath);
        }
        else
        {
            printf("failed to open dump file %s\n", dumpPath);
        }
    }

    interactor->AddObserver(vtkCommand::TimerEvent, timerCb);

    if (headlessSeconds > 0.0)
    {
        // 无窗口诊断模式：只跑取帧循环 + 诊断日志，不渲染、不进事件循环。
        // 用途：配合模拟发送端 / 真实手套验证数据链路（帧格式、帧号、坐标是否更新）。
        printf("motionGloveSDK_example_vtk headless: polling %.1f s without window...\n", headlessSeconds);
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < headlessSeconds)
        {
            timerCb->Execute(nullptr, vtkCommand::TimerEvent, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        printf("motionGloveSDK_example_vtk headless: done.\n");
        CloseUdpPortOnce();
        return listenRet != 0 ? 1 : 0;   // 无窗口模式：端口失败仍按失败退出，便于 CI 判定
    }

    if (renderCheckSeconds > 0.0)
    {
        // 离屏渲染自检：不弹窗口，按 ~60Hz 更新骨架、每秒存一张 PNG。
        // 用途：客观验证"数据在变 → 画面也在变"，避免弹窗打扰用户。
        renderWindow->SetOffScreenRendering(1);
        renderWindow->SetSize(640, 480);
        printf("motionGloveSDK_example_vtk render-check: %.1f s offscreen, out=%s\n",
               renderCheckSeconds, renderOutDir);

        const auto start = std::chrono::steady_clock::now();
        double lastCapture = -1.0;
        int shotIndex = 0;
        while (true)
        {
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= renderCheckSeconds)
            {
                break;
            }

            timerCb->Execute(nullptr, vtkCommand::TimerEvent, nullptr);   // 内部会 Render()

            if (elapsed - lastCapture >= 1.0)
            {
                lastCapture = elapsed;

                auto windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
                windowToImage->SetInput(renderWindow);
                windowToImage->SetInputBufferTypeToRGB();
                windowToImage->ReadFrontBufferOff();
                windowToImage->Update();

                char path[512];
                std::snprintf(path, sizeof(path), "%s/vtk_shot_%02d.png", renderOutDir, shotIndex++);

                auto writer = vtkSmartPointer<vtkPNGWriter>::New();
                writer->SetFileName(path);
                writer->SetInputConnection(windowToImage->GetOutputPort());
                writer->Write();
                printf("  captured %s\n", path);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        printf("motionGloveSDK_example_vtk render-check: done, %d shots.\n", shotIndex);
        CloseUdpPortOnce();
        return listenRet != 0 ? 1 : 0;
    }

    if (selfTest)
    {
        // 场景已搭建完成（渲染器 / 渲染窗口 / 交互器 / 42 个骨骼点 / 40 条连线 / 网格 / 坐标轴）。
        printf("motionGloveSDK_example_vtk self-test: scene ready (%d joints, %d bones, %d renderer actors)\n",
               kJointCount, kBoneEdgeCount, renderer->GetActors()->GetNumberOfItems());
        CloseUdpPortOnce();
        return listenRet != 0 ? 1 : 0;
    }

    // 没有 OpenGL 3.2+（如无显卡的服务器 / CI）时 VTK 会在渲染阶段崩溃，这里先探测并干净退出。
    auto glWindow = vtkOpenGLRenderWindow::SafeDownCast(renderWindow);
    if (glWindow && !glWindow->SupportsOpenGL())
    {
        printf("motionGloveSDK_example_vtk: OpenGL 3.2+ is required but not available.\n");
        const std::string supportMessage = glWindow->GetOpenGLSupportMessage();
        if (!supportMessage.empty())
        {
            printf("%s\n", supportMessage.c_str());
        }
        CloseUdpPortOnce();
        return 2;
    }

    renderWindow->Render();
    interactor->Initialize();
    interactor->CreateRepeatingTimer(16);
    interactor->Start();

    CloseUdpPortOnce();
    return 0;
}
