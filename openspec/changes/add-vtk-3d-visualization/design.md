## Context

The MotionGloveSDK is a C++11 shared library (DLL/SO) that receives UDP data frames from a MotionGlove client and exposes a 32-bone hand skeleton (16 per hand: palm + 5 fingers × 3 joints). The SDK's only output today is raw data via `MotionGloveSDK_GetGloveSkeletonsFrame`, consumed by console-printing example programs. No visual feedback exists. VTK (Visualization Toolkit) is a mature, cross-platform C++ library for 3D rendering and is a natural fit for visualizing skeletal transforms directly from the SDK's quaternion and position data.

## Goals / Non-Goals

**Goals:**
- Render a real-time 3D hand skeleton (both hands) driven by live SDK data at ~60 Hz
- Represent each bone as a visible 3D actor (cylinder or oriented line) with correct position and orientation from `SingleSkeletonDef.position_meter` and `quat_xyzw`
- Package as a self-contained optional example with a CMake opt-in flag
- Work on Windows (VS2022/VS2026) with VTK 9.x installed

**Non-Goals:**
- Anatomically detailed hand mesh or skin rendering (bones/sticks only)
- Finger-tip prediction or any signal processing beyond raw SDK data
- GUI controls, recording, or playback
- Modifying the SDK library itself
- Mandating VTK as a required dependency for all users

## Decisions

### D1: VTK as the rendering library

**Choice**: VTK 9.x
**Rationale**: VTK is open-source, well-documented, integrates cleanly via CMake `find_package(VTK)`, supports both Windows and Linux, and handles the 3D pipeline (actors, transforms, renderer, interactor) without requiring a game engine. Alternatives considered:

| Alternative | Why rejected |
|---|---|
| OpenGL + GLFW/GLUT | Requires writing a full rendering pipeline from scratch; overkill for a debug viz tool |
| Qt3D | Heavy GUI framework dependency; overkill |
| Unreal / Unity | Requires engine integration, not suitable for a standalone SDK example |
| ImGui + ImGuizmo | 2D-focused; 3D transforms are manual |

### D2: One VTK actor per bone segment

**Choice**: Each of the 32 `SingleSkeletonDef` entries gets one `vtkCylinderSource` actor. A `vtkTransform` is updated each frame from `position_meter` (translation) and `quat_xyzw` (rotation).
**Rationale**: Direct 1:1 mapping to SDK data model. Simple to implement, easy to debug per-bone. Cylinder length and radius are fixed constants (visual approximation only).
**Alternative considered**: Building a polyline skeleton by connecting parent→child joints. Rejected because the SDK provides absolute bone positions/orientations, so per-actor transforms are more straightforward and don't require a hierarchy walk.

### D3: Render loop integration

**Choice**: Use `vtkTimerCallback` on the VTK render window interactor, polling `MotionGloveSDK_isGloveNewFramePending` at each timer tick (~16 ms), updating actor transforms, and calling `renderWindow->Render()`.
**Rationale**: VTK's event loop owns the main thread. A timer callback is the idiomatic way to inject per-frame updates without blocking the interactor. The alternative (separate thread posting render events) adds synchronization complexity for no gain in a single-example tool.

### D4: Optional CMake integration

**Choice**: Add a `BUILD_VTK_EXAMPLE` CMake option (default `OFF`) in the top-level `CMakeLists.txt`. When `ON`, `find_package(VTK REQUIRED)` is called and the example target links `${VTK_LIBRARIES}`.
**Rationale**: VTK is a large optional dependency. Making it opt-in ensures existing builds are unaffected. Users who want the visualization set `-DBUILD_VTK_EXAMPLE=ON` and ensure VTK is in their CMake prefix path.

### D5: Coordinate system mapping

**Choice**: Use SDK's `position_meter` directly as VTK world-space translation. Apply `quat_xyzw` as a VTK rotation transform (convert quaternion → axis-angle for `vtkTransform::RotateWXYZ`).
**Rationale**: The SDK coordinate system (right-handed, documented in the MotionVenus 3D window) maps cleanly to VTK's default right-handed coordinate system. No axis flipping should be needed; if visual anomalies appear during testing, an optional axis remap can be added.

## Risks / Trade-offs

- **VTK version mismatch** → Mitigation: Document minimum VTK 9.x; use `find_package(VTK 9.0 REQUIRED)` to enforce version.
- **No position data from glove** → The SDK may stream gloves configured without `SkeletonPosition_Meter`, leaving `position_meter` all zeros. Mitigation: Fall back to rendering bones at fixed anatomical offsets computed from Euler angles; document this limitation.
- **60 Hz render may stutter on slow machines** → VTK software rendering is CPU-bound. Mitigation: This is a debug/demo tool; no performance guarantee needed. Timer interval is tunable.
- **Linux VTK availability** → VTK 9.x is available via apt on Ubuntu 22.04+. Mitigation: Note in README that Linux support requires `apt install libvtk9-dev`.
- **Quaternion convention** → SDK uses `xyzw` order; VTK `RotateWXYZ` expects `w, x, y, z`. Mitigation: Reorder in the transform update code.

## Open Questions

- Should both hands always render, or only the hand(s) with active data streams?
- Should bone cylinders be colored differently per finger (thumb/index/etc.) for readability?
- Is there a preferred VTK installation method to document for Windows (vcpkg, prebuilt installer, or source build)?
