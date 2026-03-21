## 1. CMake Integration

- [x] 1.1 Add `option(BUILD_VTK_EXAMPLE "Build VTK 3D visualization example" OFF)` to top-level `CMakeLists.txt`
- [x] 1.2 Add conditional `find_package(VTK 9.0 REQUIRED)` block guarded by `BUILD_VTK_EXAMPLE`
- [x] 1.3 Add `vtk_module_autoinit` and `target_link_libraries` for VTK in the examples `CMakeLists.txt` (or top-level, whichever manages examples), guarded by `BUILD_VTK_EXAMPLE`
- [x] 1.4 Verify default build (`BUILD_VTK_EXAMPLE=OFF`) still compiles without VTK installed

## 2. VTK Example Source File

- [x] 2.1 Create `MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp` with includes for VTK headers and `motionGloveSDK.h`
- [x] 2.2 Implement startup: call `MotionGloveSDK_ListenUDPPort(5001)`, print error and return on failure
- [x] 2.3 Create VTK renderer, render window, and interactor objects
- [x] 2.4 Create 32 `vtkCylinderSource` actors (one per `kinemHumanHandsSkeleton32Index`), add all to renderer with distinct colors per hand (e.g., blue = right, green = left)
- [x] 2.5 Implement `vtkTimerCallback::Execute` that polls `MotionGloveSDK_isGloveNewFramePending`, calls `MotionGloveSDK_GetGloveSkeletonsFrame`, then loops over all 32 bones updating each actor's `vtkTransform` from `position_meter` and `quat_xyzw` (reorder to w,x,y,z for `RotateWXYZ`)
- [x] 2.6 Register the timer callback on the interactor with a ~16 ms interval
- [x] 2.7 Implement exit handler (window close / 'Q' key) that calls `MotionGloveSDK_CloseUDPPort()` before exiting

## 3. Coordinate System & Visual Verification

- [ ] 3.1 Run the example with a connected glove; verify that opening/closing fingers moves the corresponding bone actors visually
- [ ] 3.2 Confirm quaternion identity (`quat_xyzw = [0,0,0,1]`) leaves actors unrotated
- [x] 3.3 If `position_meter` is all-zero (glove not configured for position), add fixed anatomical offset fallback per bone index so the hand is still recognizable

## 4. Documentation

- [x] 4.1 Add a section to `README.md` describing the VTK example: prerequisites (VTK 9.x), how to enable `BUILD_VTK_EXAMPLE=ON`, and Windows install guidance (prebuilt installer or vcpkg)
- [x] 4.2 Add inline comments in `motionGloveSDK_example_vtk.cpp` explaining the quaternion reorder, coordinate system, and timer callback pattern
