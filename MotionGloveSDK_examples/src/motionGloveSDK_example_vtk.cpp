/**
 * @file  motionGloveSDK_example_vtk.cpp
 * @brief 示例 6（可选）：基于 VTK 9 的手套骨骼三维可视化。
 * @ingroup mgsdk_examples
 *
 * @details
 * 需要 BUILD_VTK_EXAMPLE=ON 且系统安装 VTK 9.x。演示把 SDK 取回的
 * 42 段骨骼姿态映射为 VTK 场景中的几何体（圆柱/球），并用
 * vtkInteractorStyleTrackballCamera 提供交互视角。
 */
#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "motionGloveSDK.h"

#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkCylinderSource.h>
#include <vtkInteractorStyleTrackballCamera.h>
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
 * @param argc [in] 命令行参数个数（未使用）。
 * @param argv [in] 命令行参数数组（未使用）。
 * @return 0 表示正常退出；1 表示 UDP 绑定失败。
 */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

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

    renderer->ResetCamera();

    // Exit handlers: close UDP port on window close or 'Q'.
    auto exitCb = vtkSmartPointer<vtkCallbackCommand>::New();
    exitCb->SetCallback(OnExit);

    auto keyCb = vtkSmartPointer<vtkCallbackCommand>::New();
    keyCb->SetCallback(OnKeyPress);

    renderWindow->AddObserver(vtkCommand::WindowCloseEvent, exitCb);
    interactor->AddObserver(vtkCommand::ExitEvent, exitCb);
    interactor->AddObserver(vtkCommand::KeyPressEvent, keyCb);

    // Timer callback pattern: poll SDK at ~60 Hz.
    auto timerCb = vtkSmartPointer<VTKHandTimerCallback>::New();
    timerCb->SetRenderWindow(renderWindow);
    timerCb->SetTransforms(&transforms);
    timerCb->SetFallbackPositions(&fallbackPositions);

    interactor->AddObserver(vtkCommand::TimerEvent, timerCb);

    renderWindow->Render();
    interactor->Initialize();
    interactor->CreateRepeatingTimer(16);
    interactor->Start();

    CloseUdpPortOnce();
    return 0;
}
