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

static const int kDefaultUdpPort = 5001;
static const char* kDefaultGloveName = "Glove1";

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

class VTKHandTimerCallback final : public vtkCommand
{
public:
    static VTKHandTimerCallback* New() { return new VTKHandTimerCallback(); }

    void SetRenderWindow(vtkRenderWindow* rw) { this->RenderWindow = rw; }

    void SetTransforms(std::array<vtkSmartPointer<vtkTransform>, KHHS32_SkeletonCount>* transforms)
    {
        this->Transforms = transforms;
    }

    void SetFallbackPositions(std::array<std::array<double, 3>, KHHS32_SkeletonCount>* fallback)
    {
        this->FallbackPositions = fallback;
    }

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
            if (MotionGloveSDK_GetGloveSkeletonsFrame(kDefaultGloveName, &frame) == 0)
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
    vtkRenderWindow* RenderWindow = nullptr;
    std::array<vtkSmartPointer<vtkTransform>, KHHS32_SkeletonCount>* Transforms = nullptr;
    std::array<std::array<double, 3>, KHHS32_SkeletonCount>* FallbackPositions = nullptr;
};

static bool gUdpClosed = false;

static void CloseUdpPortOnce()
{
    if (!gUdpClosed)
    {
        MotionGloveSDK_CloseUDPPort();
        gUdpClosed = true;
    }
}

static void OnExit(vtkObject* caller, unsigned long eventId, void* clientData, void* callData)
{
    (void)caller;
    (void)eventId;
    (void)clientData;
    (void)callData;
    CloseUdpPortOnce();
}

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
