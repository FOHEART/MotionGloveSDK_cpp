/**
 * @file  motionGloveSDK_example_vtk.cpp
 * @brief 示例 6（可选）：基于 VTK 9 的手套骨骼三维可视化。
 * @ingroup mgsdk_examples
 *
 * @details
 * 需要 BUILD_VTK_EXAMPLE=ON 且系统安装 VTK 9.x。演示把 SDK 取回的
 * 42 段骨骼姿态映射为 VTK 场景中的几何体（圆柱/球），并用
 * vtkInteractorStyleTrackballCamera 提供交互视角。原点处绘制三色坐标轴
 * （X 红 / Y 绿 / Z 蓝）与 XZ 平面的地平面网格，作为位置参考。
 *
 * 命令行参数：
 *   --self-test   只执行初始化（UDP 绑定 + 场景搭建）后退出，用于 CI 启动自检。
 */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "motionGloveSDK.h"

#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkArrowSource.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkCylinderSource.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

/** @brief 默认绑定的 UDP 端口号。 */
static const int kDefaultUdpPort = 5001;
/** @brief 默认查询的数据流（手套）名称。 */
static const char* kDefaultGloveName = "Glove1";

/**
 * @brief 构建未收到有效位置数据前使用的"手掌状"默认布局坐标。
 * @return 32 段骨骼的默认位置数组，单位米。
 */
static std::array<std::array<double, 3>, KHHS32_SkeletonCount> BuildFallbackPositions()
{
    // Rough "hand-like" layout used before we receive valid position_meter values.
    std::array<std::array<double, 3>, KHHS32_SkeletonCount> p{};

    auto setHand = [&](int baseIndex, double baseX, double baseY, double baseZ, double thumbSide)
        {
            p[baseIndex] = { baseX, baseY, baseZ };

            auto setFinger = [&](int i1, double fx, double fy)
                {
                    p[i1] = { baseX + fx, baseY + fy, baseZ };
                    p[i1 + 1] = { baseX + fx, baseY + fy + 0.025, baseZ };
                    p[i1 + 2] = { baseX + fx, baseY + fy + 0.050, baseZ };
                };

            // Thumb / Index / Middle / Ring / Pinky (3 joints each)
            setFinger(baseIndex + 1, 0.030 * thumbSide, -0.010);
            setFinger(baseIndex + 4, 0.020, 0.020);
            setFinger(baseIndex + 7, 0.000, 0.025);
            setFinger(baseIndex + 10, -0.020, 0.020);
            setFinger(baseIndex + 13, -0.040, 0.015);
        };

    setHand(KHHS32_RightHand, 0.080, 0.000, 0.000, +1.0);
    setHand(KHHS32_LeftHand, -0.080, 0.000, 0.000, -1.0);

    return p;
}

/** @brief 地平面网格的格距，单位米。 */
static const double kGridCellMeter = 0.1;
/** @brief 地平面网格的半边长，单位米（0.25 m 即 50 x 50 cm 的范围）。 */
static const double kGridHalfExtentMeter = 0.25;
/** @brief 原点坐标轴箭头的长度，单位米。 */
static const double kAxisLengthMeter = 0.1;

/**
 * @brief 构建 XZ 平面（y = 0）上的地平面网格线。
 * @details 网格间距 0.1 m，范围 X/Z 各 50 cm（-0.25 m ~ +0.25 m），
 *          共 6 x 6 条线段，端点落在 0.1 m 的整数倍偏移上。
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
 * @brief 创建一条从原点出发的坐标轴箭头。
 * @details vtkArrowSource 默认沿 +X、长度为 1，这里先按长度缩放，
 *          再绕给定轴旋转到目标方向（Y 轴绕 Z 转 90°，Z 轴绕 Y 转 -90°）。
 * @param rgb [in] 箭头颜色，分量范围 0~1。
 * @param angleDeg [in] 旋转角，单位度。
 * @param ax/ay/az [in] 旋转轴（归一化与否均可）。
 * @return 带颜色、长度与朝向的箭头 actor。
 */
static vtkSmartPointer<vtkActor> CreateAxisActor(const double rgb[3],
                                                 double angleDeg, double ax, double ay, double az)
{
    auto arrow = vtkSmartPointer<vtkArrowSource>::New();
    arrow->SetTipResolution(24);
    arrow->SetShaftResolution(24);
    arrow->SetTipLength(0.25);
    arrow->SetTipRadius(0.08);
    arrow->SetShaftRadius(0.02);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(arrow->GetOutputPort());

    auto transform = vtkSmartPointer<vtkTransform>::New();
    transform->PostMultiply();
    transform->Scale(kAxisLengthMeter, kAxisLengthMeter, kAxisLengthMeter);
    transform->RotateWXYZ(angleDeg, ax, ay, az);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->SetUserTransform(transform);
    actor->GetProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
    return actor;
}

/**
 * @brief 把 SDK 的 xyzw 四元数转换为 VTK RotateWXYZ 需要的"角度 + 轴"表示。
 * @param q_xyzw [in] 输入四元数，顺序 xyzw。
 * @param outAngleDeg [out] 输出旋转角，单位度。
 * @param outAxis [out] 输出旋转轴（长度 3，归一化）。
 */
static void QuaternionXYWZ_ToAxisAngleDeg(const float q_xyzw[4], double& outAngleDeg, double outAxis[3])
{
    // SDK quaternion is xyzw; VTK RotateWXYZ expects angle + axis.
    double x = static_cast<double>(q_xyzw[0]);
    double y = static_cast<double>(q_xyzw[1]);
    double z = static_cast<double>(q_xyzw[2]);
    double w = static_cast<double>(q_xyzw[3]);

    const double n = std::sqrt(x * x + y * y + z * z + w * w);
    if (n > 0.0)
    {
        x /= n; y /= n; z /= n; w /= n;
    }

    w = std::max(-1.0, std::min(1.0, w));

    const double angleRad = 2.0 * std::acos(w);
    const double s = std::sqrt(std::max(0.0, 1.0 - w * w));

    if (s < 1e-8 || angleRad < 1e-8)
    {
        outAngleDeg = 0.0;
        outAxis[0] = 1.0; outAxis[1] = 0.0; outAxis[2] = 0.0;
        return;
    }

    outAngleDeg = angleRad * (180.0 / 3.14159265358979323846);
    outAxis[0] = x / s;
    outAxis[1] = y / s;
    outAxis[2] = z / s;
}

/**
 * @brief 判断一帧数据的全部骨骼位置是否均为零（未收到有效位置的判定依据）。
 * @param frame [in] 待检查的帧数据。
 * @return true 表示全部位置接近零；false 表示至少一段位置有效。
 */
static bool FramePositionsAllZero(const KHHS32PosAttitude_TypeDef& frame)
{
    for (int i = 0; i < static_cast<int>(KHHS32_SkeletonCount); ++i)
    {
        const float* p = frame.skeletons[i].position_meter;
        if (std::fabs(p[0]) > 1e-6f || std::fabs(p[1]) > 1e-6f || std::fabs(p[2]) > 1e-6f)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief VTK 定时器回调：每帧从 SDK 取最新骨骼姿态并刷新场景。
 * @details 挂接在 vtkRenderWindowInteractor 的 TimerEvent 上；
 *          RenderWindow 用于触发重绘，帧数据经 SDK 查询接口获取。
 */
class VTKHandTimerCallback final : public vtkCommand
{
public:
    /** @brief VTK 标准工厂方法：创建回调实例（等价于 new + Delete 计数管理）。 @return 新实例指针。 */
    static VTKHandTimerCallback* New() { return new VTKHandTimerCallback(); }

    /** @brief 设置用于触发重绘的渲染窗口。 @param rw [in] 目标渲染窗口指针。 */
    void SetRenderWindow(vtkRenderWindow* rw) { this->RenderWindow = rw; }

    /** @brief 设置骨骼变换数组（每段骨骼一个 vtkTransform）。 @param transforms [in] 变换数组指针。 */
    void SetTransforms(std::array<vtkSmartPointer<vtkTransform>, KHHS32_SkeletonCount>* transforms)
    {
        this->Transforms = transforms;
    }

    /** @brief 设置未收到有效位置时使用的默认布局坐标。 @param fallback [in] 默认坐标数组指针。 */
    void SetFallbackPositions(std::array<std::array<double, 3>, KHHS32_SkeletonCount>* fallback)
    {
        this->FallbackPositions = fallback;
    }

    /**
     * @brief VTK 事件处理：TimerEvent 到来时从 SDK 取最新帧并更新各骨骼变换。
     * @param caller [in] 事件来源对象（未使用）。
     * @param eventId [in] 事件编号（仅处理 TimerEvent）。
     * @param callData [in] 事件附加数据（未使用）。
     */
    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override
    {
        (void)caller;
        (void)callData;

        if (eventId != vtkCommand::TimerEvent || this->RenderWindow == nullptr || this->Transforms == nullptr)
        {
            return;
        }

        if (MotionGloveSDK_isGloveNewFramePending(kDefaultGloveName))
        {
            KHHS32PosAttitude_TypeDef frame;
            if (MotionGloveSDK_GetSkeletons32Frame(kDefaultGloveName, &frame) == 0)
            {
                const bool allZeroPos = FramePositionsAllZero(frame);

                for (int i = 0; i < static_cast<int>(KHHS32_SkeletonCount); ++i)
                {
                    const float* pos = frame.skeletons[i].position_meter;
                    double px = static_cast<double>(pos[0]);
                    double py = static_cast<double>(pos[1]);
                    double pz = static_cast<double>(pos[2]);

                    if (allZeroPos && this->FallbackPositions != nullptr)
                    {
                        px = (*this->FallbackPositions)[i][0];
                        py = (*this->FallbackPositions)[i][1];
                        pz = (*this->FallbackPositions)[i][2];
                    }

                    double angleDeg = 0.0;
                    double axis[3] = { 1.0, 0.0, 0.0 };
                    QuaternionXYWZ_ToAxisAngleDeg(frame.skeletons[i].quat_xyzw, angleDeg, axis);

                    vtkTransform* t = (*this->Transforms)[i];
                    t->Identity();
                    t->Translate(px, py, pz);
                    if (angleDeg != 0.0)
                    {
                        t->RotateWXYZ(angleDeg, axis[0], axis[1], axis[2]);
                    }
                }

                MotionGloveSDK_resetGloveNewFramePending(kDefaultGloveName);
            }
        }

        // Timer callback pattern: let VTK own the event loop; we simply update state and request a render.
        this->RenderWindow->Render();
    }

private:
    /** @brief 触发重绘的渲染窗口（nullptr 表示未设置）。 */
    vtkRenderWindow* RenderWindow = nullptr;
    /** @brief 骨骼变换数组指针，随帧更新旋转。 */
    std::array<vtkSmartPointer<vtkTransform>, KHHS32_SkeletonCount>* Transforms = nullptr;
    /** @brief 默认布局坐标指针，位置全零时使用。 */
    std::array<std::array<double, 3>, KHHS32_SkeletonCount>* FallbackPositions = nullptr;
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
 * @brief 程序入口：绑定 UDP 端口后启动 VTK 3D 窗口，实时渲染 42 段手部骨骼。
 * @details 支持 `--self-test`：只走完初始化路径（UDP 绑定 + VTK 场景搭建）就退出，
 *          不创建 OpenGL 上下文、不进事件循环，便于在无显卡的 CI 环境做启动自检。
 * @param argc [in] 命令行参数个数。
 * @param argv [in] 命令行参数数组。
 * @return 0 表示正常退出；1 表示 UDP 绑定失败。
 */
int main(int argc, char* argv[])
{
    bool selfTest = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--self-test") == 0)
        {
            selfTest = true;
        }
    }
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    printf("motionGloveSDK_example_vtk: binding UDP 127.0.0.1:%d\n", kDefaultUdpPort);
    const int listenRet = MotionGloveSDK_ListenUDPPort(kDefaultUdpPort);
    if (listenRet != 0)
    {
        printf("Failed to bind UDP port %d (code=%d). Is the port already in use?\n", kDefaultUdpPort, listenRet);
        return 1;
    }

    // VTK window should appear immediately, even before any glove frame is received.
    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.08, 0.08, 0.10);

    // 原点参考系：XZ 平面（y = 0）的地平面网格 + 三色坐标轴（X 红 / Y 绿 / Z 蓝）。
    const double axisColorX[3] = { 0.90, 0.25, 0.25 };
    const double axisColorY[3] = { 0.25, 0.85, 0.30 };
    const double axisColorZ[3] = { 0.30, 0.50, 1.00 };

    auto gridMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    gridMapper->SetInputData(BuildGroundGrid());

    auto gridActor = vtkSmartPointer<vtkActor>::New();
    gridActor->SetMapper(gridMapper);
    gridActor->GetProperty()->SetColor(0.45, 0.45, 0.50);
    gridActor->GetProperty()->SetLineWidth(1.0f);
    renderer->AddActor(gridActor);

    renderer->AddActor(CreateAxisActor(axisColorX, 0.0, 0.0, 0.0, 1.0));
    renderer->AddActor(CreateAxisActor(axisColorY, 90.0, 0.0, 0.0, 1.0));
    renderer->AddActor(CreateAxisActor(axisColorZ, -90.0, 0.0, 1.0, 0.0));

    auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("MotionGloveSDK VTK Example");
    renderWindow->SetSize(1000, 800);

    auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    interactor->SetInteractorStyle(style);

    auto cylinder = vtkSmartPointer<vtkCylinderSource>::New();
    cylinder->SetRadius(0.004);
    cylinder->SetHeight(0.030);
    cylinder->SetResolution(18);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(cylinder->GetOutputPort());

    std::array<vtkSmartPointer<vtkActor>, KHHS32_SkeletonCount> actors;
    std::array<vtkSmartPointer<vtkTransform>, KHHS32_SkeletonCount> transforms;
    auto fallbackPositions = BuildFallbackPositions();

    for (int i = 0; i < static_cast<int>(KHHS32_SkeletonCount); ++i)
    {
        transforms[i] = vtkSmartPointer<vtkTransform>::New();
        transforms[i]->PostMultiply();

        actors[i] = vtkSmartPointer<vtkActor>::New();
        actors[i]->SetMapper(mapper);
        actors[i]->SetUserTransform(transforms[i]);

        // Distinct colors per hand for readability: right = blue, left = green.
        if (i < 16)
        {
            actors[i]->GetProperty()->SetColor(0.20, 0.55, 1.00);
        }
        else
        {
            actors[i]->GetProperty()->SetColor(0.20, 0.90, 0.40);
        }

        const auto& fp = fallbackPositions[i];
        transforms[i]->Identity();
        transforms[i]->Translate(fp[0], fp[1], fp[2]);

        renderer->AddActor(actors[i]);
    }

    // 以 Y 轴为竖直方向，从斜上方俯视原点，便于看清地平面网格与三条坐标轴。
    auto camera = renderer->GetActiveCamera();
    camera->SetPosition(0.45, 0.35, 0.45);
    camera->SetFocalPoint(0.0, 0.0, 0.0);
    camera->SetViewUp(0.0, 1.0, 0.0);

    renderer->ResetCamera();

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
    timerCb->SetTransforms(&transforms);
    timerCb->SetFallbackPositions(&fallbackPositions);

    interactor->AddObserver(vtkCommand::TimerEvent, timerCb);

    if (selfTest)
    {
        // 场景已搭建完成（渲染器 / 渲染窗口 / 交互器 / 42 个骨骼 actor / 网格 / 坐标轴）。
        printf("motionGloveSDK_example_vtk self-test: scene ready (%d skeleton actors, %d renderer actors)\n",
               static_cast<int>(KHHS32_SkeletonCount), renderer->GetActors()->GetNumberOfItems());
        CloseUdpPortOnce();
        return 0;
    }

    renderWindow->Render();
    interactor->Initialize();
    interactor->CreateRepeatingTimer(16);
    interactor->Start();

    CloseUdpPortOnce();
    return 0;
}
