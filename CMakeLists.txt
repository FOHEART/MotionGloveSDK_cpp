# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# 发布版本号以 MotionGloveSDK/include/motionGloveSDK_version.h 为唯一来源，
# 发布工作流会校验它与 tag 一致。
set(MGSDK_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/MotionGloveSDK/include/motionGloveSDK_version.h")
file(READ "${MGSDK_VERSION_HEADER}" MGSDK_VERSION_CONTENT)
foreach(_part MAJOR MINOR PATCH)
    if(NOT MGSDK_VERSION_CONTENT MATCHES "#define[ \t]+MOTIONGLOVE_SDK_VER_${_part}[ \t]+([0-9]+)")
        message(FATAL_ERROR "无法从 ${MGSDK_VERSION_HEADER} 解析 MOTIONGLOVE_SDK_VER_${_part}")
    endif()
    set(MGSDK_VERSION_${_part} "${CMAKE_MATCH_1}")
endforeach()
set(MGSDK_VERSION "${MGSDK_VERSION_MAJOR}.${MGSDK_VERSION_MINOR}.${MGSDK_VERSION_PATCH}")

# 设置项目名称
project(MotionGloveSDK_examples VERSION ${MGSDK_VERSION} LANGUAGES C CXX)
message(STATUS "MotionGloveSDK 发布版本: ${MGSDK_VERSION}")

# ── 输出目录约定：所有生成物进 out/，源码树只放源码 ────────────────────────
#   out/build/x64-<配置>   CLI 构建树（scripts\build.bat）
#   out/build/vscode       IDE 构建树（VSCode CMake Tools / Presets），与 CLI 分目录
#   out/install/<配置>     安装目录（库 + 头文件）
#   out/pack               压缩包（源码分发包）
set(MGSDK_OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/out" CACHE PATH "所有构建/安装/打包生成物的根目录")

# 只在用户没有显式传 -DCMAKE_INSTALL_PREFIX 时改默认值，避免覆盖命令行
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${MGSDK_OUTPUT_ROOT}/install"
        CACHE PATH "Install prefix (out/install)" FORCE)
endif()
message(STATUS "构建输出根目录: ${MGSDK_OUTPUT_ROOT}")
message(STATUS "安装前缀: ${CMAKE_INSTALL_PREFIX}")

# 关键：添加库的子目录，这将引导CMake去执行 子目录下的 CMakeLists.txt
add_subdirectory(MotionGloveSDK)        
# 只保留 Debug 和 Release 配置
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to only Debug and Release" FORCE)
endif()
# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# VTK 3D visualization example (requires VTK 9.x)
# 默认开启：找到 VTK 就一起编译；没装 VTK 时告警并自动跳过该示例，不影响其它目标。
# 不想编译该示例时显式传 -DBUILD_VTK_EXAMPLE=OFF。
option(BUILD_VTK_EXAMPLE "Build VTK 3D visualization example (requires VTK 9.x)" ON)
if(BUILD_VTK_EXAMPLE)
    # 只请求示例实际用到的模块：既缩短链接时间，也避免依赖预编译 VTK
    # 安装包中与本示例无关的第三方模块。
    find_package(VTK 9.0 QUIET COMPONENTS
        CommonCore
        CommonDataModel
        CommonTransforms
        FiltersSources
        InteractionStyle
        IOImage
        RenderingCore
        RenderingOpenGL2
        RenderingAnnotation
        RenderingUI
    )
    if(VTK_FOUND)
        message(STATUS "VTK found: ${VTK_VERSION}")
    else()
        message(WARNING
            "未找到 VTK 9.x，已自动关闭 BUILD_VTK_EXAMPLE，motionGloveSDK_example_vtk 将不编译。\n"
            "  需要该示例时：设置 VTK_DIR 指向 vtk-config.cmake 所在目录，"
            "或运行 tools/setup_vtk.bat（Windows）后重新配置。")
        set(BUILD_VTK_EXAMPLE OFF)
    endif()
endif()

# 查找所有匹配特定模式（例如 *example*.cpp）的源文件
# 其中 motionGloveSDK_example_vtk.cpp 需要 VTK：由 BUILD_VTK_EXAMPLE 控制是否编译，
# 其余示例始终编译。
file(GLOB MAIN_SOURCES "MotionGloveSDK_examples/src/*example*.cpp")

# 提示用户输入或选择MotionGloveSDK库的根目录
set(MotionGloveSDK_ROOT "${CMAKE_SOURCE_DIR}/MotionGloveSDK" CACHE PATH "根目录路径（包含include、lib等子目录）")
if("${MotionGloveSDK_ROOT}" STREQUAL "")
    message(FATAL_ERROR "请通过 -DTHIRDPARTY_ROOT=/path/to/lib 指定第三方库的根目录。")
endif()
message(STATUS "MotionGloveSDK库根目录: ${MotionGloveSDK_ROOT}")

# 遍历找到的每个源文件
foreach(source_file IN LISTS MAIN_SOURCES)
   
    # 获取不带路径和扩展名的纯文件名
    get_filename_component(executable_name ${source_file} NAME_WE)

    # motionGloveSDK_example_vtk 依赖 VTK，未开启 BUILD_VTK_EXAMPLE 时跳过
    set(_needs_vtk FALSE)
    if(source_file MATCHES ".*_vtk\\.cpp$")
        set(_needs_vtk TRUE)
        if(NOT BUILD_VTK_EXAMPLE)
            message(STATUS "Skipped executable target: ${executable_name} (需要 VTK 9.x，原因见上面的提示)")
            continue()
        endif()
    endif()

     # 在循环内打印当前添加的目标名
    message(STATUS "Added executable target: ${executable_name}")
    # 为每个源文件创建一个可执行目标
    add_executable(${executable_name} ${source_file})

    # 每个示例目标的编译定义
    target_compile_definitions(${executable_name} PRIVATE UNICODE _UNICODE)

    # 编译器特定选项
    if (MSVC)
        target_compile_options(${executable_name} PRIVATE /utf-8 /MP /Zc:wchar_t /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${executable_name} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
    # 如果需要，可以在这里为特定目标链接库
    # target_link_libraries(${executable_name} some_library)

    # 引入MotionGloveSDK库
    target_link_libraries(${executable_name} PRIVATE MotionGloveSDK)
    target_include_directories(${executable_name} PRIVATE "${MotionGloveSDK_ROOT}/include")

    # VTK 示例额外链接 VTK 模块；autoinit 宏负责把工厂/渲染模块接起来
    if(_needs_vtk)
        target_link_libraries(${executable_name} PRIVATE ${VTK_LIBRARIES})
        vtk_module_autoinit(
            TARGETS ${executable_name}
            MODULES ${VTK_LIBRARIES}
        )
    endif()

    # add_library(MotionGloveSDK SHARED IMPORTED) 根据配置不同引入不同的库和dll
    # 说明：下面的 IMPORTED_LOCATION_* 属性只对 IMPORTED 目标生效；这里的
    # MotionGloveSDK 是 add_subdirectory 构建出的普通目标，链接时直接用
    # $<TARGET_FILE:MotionGloveSDK>，因此不需要、也不应再指向源码树下的 build/ 目录
    # （历史上这里指向 ${MotionGloveSDK_ROOT}/build/...，容易让人误以为要在根目录生成 build）。
    # 所有构建产物统一放在 out/build 下。

    
    # 将生成后的MotionGloveSDK库，自动拷贝到exe运行目录
    if(WIN32)
        add_custom_command(TARGET ${executable_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:MotionGloveSDK>"
            "$<TARGET_FILE_DIR:${executable_name}>"
        )

        # VTK 示例：把 VTK 模块及其第三方依赖的 DLL 复制到输出目录，脱离 PATH 也能直接运行。
        if(_needs_vtk AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
            # VTK 9.3+ 的导入目标带命名空间，早期版本为 vtkXxx
            set(_vtk_ref_target "")
            if(TARGET VTK::RenderingCore)
                set(_vtk_ref_target VTK::RenderingCore)
            elseif(TARGET vtkRenderingCore)
                set(_vtk_ref_target vtkRenderingCore)
            endif()

            set(VTK_RUNTIME_DIR "")
            if(_vtk_ref_target)
                get_target_property(_vtk_module_dll ${_vtk_ref_target} IMPORTED_LOCATION)
                if(NOT _vtk_module_dll)
                    get_target_property(_vtk_module_dll ${_vtk_ref_target} IMPORTED_LOCATION_RELEASE)
                endif()
                if(NOT _vtk_module_dll)
                    get_target_property(_vtk_module_dll ${_vtk_ref_target} IMPORTED_LOCATION_DEBUG)
                endif()
                if(_vtk_module_dll)
                    get_filename_component(VTK_RUNTIME_DIR "${_vtk_module_dll}" DIRECTORY)
                endif()
            endif()

            if(VTK_RUNTIME_DIR)
                add_custom_command(TARGET ${executable_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND}
                        "-DEXE=$<TARGET_FILE:${executable_name}>"
                        "-DDEST=$<TARGET_FILE_DIR:${executable_name}>"
                        "-DSEARCH_DIRS=${VTK_RUNTIME_DIR}"
                        -P "${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
                    COMMENT "Copying VTK runtime DLLs for ${executable_name}"
                    VERBATIM
                )
            endif()
        endif()
    elseif(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  
    endif()

endforeach()

if(WIN32)
# Make VS .vcxproj.filters mirror the real directory tree from the repo root
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES     ${SRC_FILES}    )
endif()


# ── 无头命令行（CLI-Anything）────────────────────────────────────────────────
# 独立薄 exe：只解析参数 + 调 SDK + 输出统一 JSON 信封，不依赖 GUI/窗口环境。
# 不想编译时传 -DBUILD_CLI=OFF。
option(BUILD_CLI "Build motionGloveSDK_cli (headless CLI, no GUI)" ON)
if(BUILD_CLI)
    add_subdirectory(cli)
endif()

# ── 单元测试（GoogleTest / TDD）─────────────────────────────────────────────
# 测试目录自带 GoogleTest 接入（FetchContent），只测算纯逻辑层
# （eulerToQuat / decodeAsGloveCSV / readWriteLock），不依赖 UDP 与窗口环境。
option(BUILD_TESTS "Build MotionGloveSDK unit tests (GoogleTest)" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()

