# 把可执行文件依赖的非系统运行库（VTK 模块及其第三方依赖）复制到可执行文件所在目录，
# 这样示例无需设置 PATH 即可直接双击运行。
#
# 由 CMakeLists.txt 的 POST_BUILD 调用，输入变量：
#   EXE          可执行文件路径
#   DEST         复制目标目录
#   SEARCH_DIRS  额外的运行库搜索目录（分号分隔，例如 VTK 的 bin 目录）
#
# 注：解析前先把 exe 复制到空的暂存目录再解析依赖，避免重复构建时目标目录里
#     已复制的同名 DLL 与搜索目录冲突（CMake 会报 Multiple conflicting paths）。

if(NOT EXISTS "${EXE}")
    return()
endif()

get_filename_component(_dest_dir "${DEST}" ABSOLUTE)
get_filename_component(_exe_name "${EXE}" NAME)

set(_stage_dir "${_dest_dir}/.vtk-deps-stage")
file(REMOVE_RECURSE "${_stage_dir}")
file(MAKE_DIRECTORY "${_stage_dir}")
file(COPY "${EXE}" DESTINATION "${_stage_dir}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_stage_dir}/${_exe_name}"
    RESOLVED_DEPENDENCIES_VAR _resolved_deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
    DIRECTORIES ${SEARCH_DIRS}
    PRE_EXCLUDE_REGEXES "api-ms-win-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ss][Yy][Ss][Tt][Ee][Mm]32.*")

foreach(_dep IN LISTS _resolved_deps)
    get_filename_component(_dep_name "${_dep}" NAME)
    if(NOT EXISTS "${_dest_dir}/${_dep_name}")
        file(COPY "${_dep}" DESTINATION "${_dest_dir}")
    endif()
endforeach()

file(REMOVE_RECURSE "${_stage_dir}")

if(_unresolved_deps)
    message(STATUS "Unresolved runtime dependencies (usually the project DLL, copied separately): ${_unresolved_deps}")
endif()
