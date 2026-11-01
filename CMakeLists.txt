# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# 设置项目名称
project(MotionGloveSDK_examples VERSION 1.0 LANGUAGES C CXX)
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


# Optional VTK 3D visualization example (requires VTK 9.x)
option(BUILD_VTK_EXAMPLE "Build VTK 3D visualization example (requires VTK 9.x)" OFF)
if(BUILD_VTK_EXAMPLE)
    find_package(VTK 9.0 REQUIRED)
    message(STATUS "VTK found: ${VTK_VERSION}")
endif()

# 查找所有匹配特定模式（例如 *example*.cpp）的源文件
# VTK example is excluded here and handled separately below
file(GLOB MAIN_SOURCES "MotionGloveSDK_examples/src/*example*.cpp")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*_vtk\\.cpp$")

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

    # add_library(MotionGloveSDK SHARED IMPORTED) 根据配置不同引入不同的库和dll
   
    if(WIN32)
        # Windows配置
        set_target_properties(${executable_name} PROPERTIES
            IMPORTED_LOCATION_DEBUG "${MotionGloveSDK_ROOT}/build/Debug/MotionGloveSDK.dll"
            IMPORTED_IMPLIB_DEBUG "${MotionGloveSDK_ROOT}/build/Debug/MotionGloveSDK.lib"
            IMPORTED_LOCATION_RELEASE "${MotionGloveSDK_ROOT}/build/Release/MotionGloveSDK.dll"
            IMPORTED_IMPLIB_RELEASE "${MotionGloveSDK_ROOT}/build/Release/MotionGloveSDK.lib"
        )
    elseif(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux/Ubuntu配置
        set_target_properties(${executable_name} PROPERTIES
            IMPORTED_LOCATION_DEBUG "${MotionGloveSDK_ROOT}/build/Debug/libMotionGloveSDK.so"
            IMPORTED_LOCATION_RELEASE "${MotionGloveSDK_ROOT}/build/Release/libMotionGloveSDK.so"
        )
    endif()

    
    # 将生成后的MotionGloveSDK库，自动拷贝到exe运行目录
    if(WIN32)
        add_custom_command(TARGET ${executable_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:MotionGloveSDK>"
            "$<TARGET_FILE_DIR:${executable_name}>"
        )
    elseif(UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  
    endif()

endforeach()

if(WIN32)
# Make VS .vcxproj.filters mirror the real directory tree from the repo root
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES     ${SRC_FILES}    )
endif()

# ── Optional VTK 3D visualization example ──────────────────────────────────
if(BUILD_VTK_EXAMPLE)
    set(VTK_EXAMPLE_SRC "MotionGloveSDK_examples/src/motionGloveSDK_example_vtk.cpp")
    set(VTK_EXAMPLE_TARGET "motionGloveSDK_example_vtk")

    add_executable(${VTK_EXAMPLE_TARGET} ${VTK_EXAMPLE_SRC})
    message(STATUS "Added executable target: ${VTK_EXAMPLE_TARGET}")

    target_compile_definitions(${VTK_EXAMPLE_TARGET} PRIVATE UNICODE _UNICODE)

    if(MSVC)
        target_compile_options(${VTK_EXAMPLE_TARGET} PRIVATE /utf-8 /MP /Zc:wchar_t /D_CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${VTK_EXAMPLE_TARGET} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()

    target_link_libraries(${VTK_EXAMPLE_TARGET} PRIVATE MotionGloveSDK ${VTK_LIBRARIES})
    target_include_directories(${VTK_EXAMPLE_TARGET} PRIVATE "${MotionGloveSDK_ROOT}/include")

    # VTK requires autoinit macros to wire up factory/rendering modules
    vtk_module_autoinit(
        TARGETS ${VTK_EXAMPLE_TARGET}
        MODULES ${VTK_LIBRARIES}
    )

    if(WIN32)
        add_custom_command(TARGET ${VTK_EXAMPLE_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:MotionGloveSDK>"
            "$<TARGET_FILE_DIR:${VTK_EXAMPLE_TARGET}>"
        )
    endif()
endif()

# ── 单元测试（GoogleTest / TDD）─────────────────────────────────────────────
# 测试目录自带 GoogleTest 接入（FetchContent），只测算纯逻辑层
# （eulerToQuat / decodeAsGloveCSV / readWriteLock），不依赖 UDP 与窗口环境。
option(BUILD_TESTS "Build MotionGloveSDK unit tests (GoogleTest)" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()

