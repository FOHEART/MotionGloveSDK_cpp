## Why

The SDK currently only provides raw data access APIs and text-based console examples. Developers and researchers have no way to visually verify glove data in real time, making debugging and demonstration difficult. Introducing VTK-based 3D visualization provides an interactive hand model that renders live skeletal pose data, making the SDK significantly more useful for evaluation and demonstration.

## What Changes

- New standalone example program (`motionGloveSDK_example_vtk`) that opens a VTK window and renders a 3D hand skeleton model driven by live glove data
- Two rendered hand models (left + right), each consisting of 16 bone segments (hand palm + 5 fingers × 3 joints)
- Bone segments represented as VTK cylinder/line actors connected at anatomically correct joint positions
- Real-time update loop: SDK frame data → bone transform (position + quaternion rotation) → VTK actor transform, refreshed each frame (~60 Hz)
- CMake option `BUILD_VTK_EXAMPLE` (OFF by default) to optionally include the VTK example; VTK is found via `find_package(VTK REQUIRED)`
- No changes to the existing SDK library (`MotionGloveSDK`) or existing examples

## Capabilities

### New Capabilities
- `vtk-hand-renderer`: Real-time 3D hand skeleton visualization using VTK, consuming `KHHS32PosAttitude_TypeDef` frame data and rendering 32 bone actors for left and right hands

### Modified Capabilities
<!-- None — no existing specs or SDK behavior changes -->

## Impact

- **New dependency**: VTK (Visualization Toolkit), version 9.x recommended; must be installed separately by the user
- **New files**: `MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp` + supporting CMake changes
- **CMake**: Top-level `CMakeLists.txt` and/or examples `CMakeLists.txt` updated with optional VTK find/link
- **No API changes**: Existing SDK public headers and library unchanged
- **Platforms**: Primarily Windows (VS2022/VS2026); Linux support contingent on VTK availability
