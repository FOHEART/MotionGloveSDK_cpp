## ADDED Requirements

### Requirement: VTK 3D window launches on startup
The example program SHALL open a VTK render window displaying a 3D viewport upon launch, before any glove data is received.

#### Scenario: Window opens successfully
- **WHEN** the user runs `motionGloveSDK_example_vtk` with VTK libraries present
- **THEN** a VTK render window with a dark background and a 3D camera view SHALL appear within 2 seconds

#### Scenario: Missing VTK prevents build
- **WHEN** `BUILD_VTK_EXAMPLE=ON` is set but VTK is not installed
- **THEN** CMake configuration SHALL fail with a clear error message indicating VTK is missing

---

### Requirement: UDP port binding on launch
The example SHALL bind UDP port 5001 (default) to receive glove data using `MotionGloveSDK_ListenUDPPort`.

#### Scenario: Successful bind
- **WHEN** the program starts and port 5001 is available
- **THEN** `MotionGloveSDK_ListenUDPPort(5001)` SHALL return 0 and data reception SHALL begin

#### Scenario: Port already in use
- **WHEN** the program starts and port 5001 is already in use
- **THEN** the program SHALL print an error message and exit gracefully without crashing

---

### Requirement: 32 bone actors rendered for both hands
The renderer SHALL create exactly 32 VTK actors (one per `kinemHumanHandsSkeleton32Index` entry): 16 for the right hand and 16 for the left hand.

#### Scenario: Initial pose at rest
- **WHEN** no glove data has been received yet
- **THEN** all 32 bone actors SHALL be visible at default positions forming a rough hand-like layout

#### Scenario: Actor count matches skeleton count
- **WHEN** the VTK renderer is initialized
- **THEN** the number of actors added to the renderer SHALL equal `KHHS32_SkeletonCount` (32)

---

### Requirement: Real-time bone transform update from SDK data
Each rendered bone actor's position and orientation SHALL update every frame using `position_meter[3]` (translation) and `quat_xyzw[4]` (rotation) from `SingleSkeletonDef`.

#### Scenario: Live data drives bone movement
- **WHEN** the glove hardware moves a finger
- **THEN** the corresponding bone actor(s) in the VTK window SHALL visually reflect the movement within one render frame (~16 ms)

#### Scenario: Quaternion applied correctly
- **WHEN** a bone's `quat_xyzw` is `[0, 0, 0, 1]` (identity)
- **THEN** the bone actor SHALL display with no rotation (aligned to default cylinder axis)

#### Scenario: Frame rate maintenance
- **WHEN** glove data arrives at 60 Hz
- **THEN** the VTK render window SHALL update at a rate of at least 30 Hz

---

### Requirement: Optional CMake build flag
The VTK example SHALL only be compiled when the CMake option `BUILD_VTK_EXAMPLE` is set to `ON`.

#### Scenario: Default build excludes VTK example
- **WHEN** the project is built without specifying `BUILD_VTK_EXAMPLE`
- **THEN** no VTK-related CMake targets or `find_package(VTK)` calls SHALL be executed

#### Scenario: Opt-in build includes VTK example
- **WHEN** `-DBUILD_VTK_EXAMPLE=ON` is passed to CMake and VTK 9.x is installed
- **THEN** the `motionGloveSDK_example_vtk` target SHALL be configured and built successfully

---

### Requirement: Graceful window close and cleanup
When the user closes the VTK window or presses 'Q', the program SHALL release the UDP port and exit cleanly.

#### Scenario: User closes window
- **WHEN** the user closes the VTK render window
- **THEN** `MotionGloveSDK_CloseUDPPort()` SHALL be called and the process SHALL exit with code 0
