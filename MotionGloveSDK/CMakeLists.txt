# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# 设置项目名称
set(MGSDK_PROJECT_NAME MotionGloveSDK)
project(${MGSDK_PROJECT_NAME} VERSION 1.0 LANGUAGES C CXX)
# 只保留 Debug 和 Release 配置
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to only Debug and Release" FORCE)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 手动指定源文件（请根据您的实际文件修改以下路径）
# CPP文件
set(SRC_FILES
src/decodeAsGloveCSV.cpp
src/eulerToQuat.cpp
"src/motionGloveSDK.cpp"
"src/motionGloveSdkHelper.cpp"

    # 添加其他.cpp文件
)

# 头文件
set(HEADER_FILES
"include/motionGloveSDK.h"
"include/motionGloveSDK_commonDef.h"
"include/motionGloveSDK_HMAXGloveDef.h"
"include/motionGloveSDKDef.h"
inc/decodeAsGloveCSV.h
inc/eulerToQuat.h
"inc/motionGloveSdkHelper.h"
inc/readWriteLock.h

    # 添加其他.h文件
)

# 创建可执行文件，包含所有手动指定的文件
add_library(${MGSDK_PROJECT_NAME} SHARED
    ${SRC_FILES}
    ${HEADER_FILES}
)
# 指定头文件目录，以便编译器能够找到
target_include_directories(${MGSDK_PROJECT_NAME} PUBLIC  ${PROJECT_SOURCE_DIR})

if(WIN32)
# Make VS .vcxproj.filters mirror the real directory tree from the repo root
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES     ${SRC_FILES}     ${HEADER_FILES})
endif()

target_compile_definitions(${MGSDK_PROJECT_NAME} PRIVATE MGSDK_DLL_EXPORT)
# 全局预处理定义（使代码以 Unicode 编译）
target_compile_definitions(${MGSDK_PROJECT_NAME} PRIVATE UNICODE _UNICODE)

# 编译器特定选项
if (MSVC)
    target_compile_options(${MGSDK_PROJECT_NAME} PRIVATE /utf-8 /MP /Zc:wchar_t /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN)
else()
    target_compile_options(${MGSDK_PROJECT_NAME} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# 在 Windows 上使 Visual Studio 的 .vcxproj.filters 显示正确的目录结构
if(WIN32)
    source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${SRC_FILES} ${HEADER_FILES})
endif()

# Install
include(GNUInstallDirs)

# Install library (DLL + import library on Windows)
install(TARGETS ${MGSDK_PROJECT_NAME}
    EXPORT ${MGSDK_PROJECT_NAME}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install public headers: copy the repository's `include/` folder to <prefix>/include
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

