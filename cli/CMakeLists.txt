# ── motionGloveSDK_cli：无头命令行（CLI-Anything） ──────────────────────────
#
# 设计要点：
#  1. 独立薄 CLI exe：控制台子系统，不依赖任何 GUI/窗口/图形运行时；
#  2. 只 link 主工程的 MotionGloveSDK 共享库，通过公开 C 接口取帧，
#     不复制任何业务逻辑；
#  3. 所有命令共用统一 JSON 信封（cli_parser.cpp），`--json` 全局可选；
#  4. 产物与示例同目录，方便 deploy.bat 一并汇总、agent-harness 自动定位。

set(MGSDK_CLI_NAME motionGloveSDK_cli)

if(NOT DEFINED MotionGloveSDK_ROOT)
    set(MotionGloveSDK_ROOT "${PROJECT_SOURCE_DIR}/MotionGloveSDK")
endif()

add_executable(${MGSDK_CLI_NAME}
    main.cpp
    cli_parser.cpp
    cli_parser.h
    session.cpp
    session.h
    sdk_info.cpp
    sdk_info.h
    commands/run_cmd.cpp
    commands/run_cmd.h
    commands/export_cmd.cpp
    commands/export_cmd.h
    commands/status_cmd.cpp
    commands/status_cmd.h
    commands/config_cmd.cpp
    commands/config_cmd.h
)

target_include_directories(${MGSDK_CLI_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MotionGloveSDK_ROOT}/include"
    "${PROJECT_SOURCE_DIR}/MotionGloveSDK_examples/inc"
)

target_compile_definitions(${MGSDK_CLI_NAME} PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(${MGSDK_CLI_NAME} PRIVATE /utf-8 /MP /Zc:wchar_t /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(${MGSDK_CLI_NAME} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# std::this_thread::sleep_for 在 Linux 下需要 Threads（Windows 下为空）
find_package(Threads REQUIRED)
target_link_libraries(${MGSDK_CLI_NAME} PRIVATE MotionGloveSDK Threads::Threads)

# 产物与示例同目录（多配置：<build>/<Config>；单配置：<build>），
# 这样 scripts/deploy.bat 的"汇总示例"步骤能一并把 CLI 拷到安装目录。
if(CMAKE_CONFIGURATION_TYPES)
    set_target_properties(${MGSDK_CLI_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
else()
    set_target_properties(${MGSDK_CLI_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# 与示例保持一致：Windows 下把 SDK 运行库拷到 exe 同目录（多配置生成器本就在同一目录，
# 这里只是让单配置/自定义输出目录也能直接运行）。
if(WIN32)
    add_custom_command(TARGET ${MGSDK_CLI_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MotionGloveSDK>"
        "$<TARGET_FILE_DIR:${MGSDK_CLI_NAME}>"
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${MGSDK_CLI_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
