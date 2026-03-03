cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

# 设置工具集为v142 (Visual Studio 2019) 以避免v143工具集缺失的问题
set(CMAKE_GENERATOR_TOOLSET "v142" CACHE STRING "Platform toolset" FORCE)

project(CppPractices VERSION 1.0)
message(STATUS "[Project] ${PROJECT_NAME} ${PROJECT_VERSION}")

include(cmake/helper.cmake)

set(OUTPUT_PROP   
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/bin"
)

add_subdirectory(src)