cmake_minimum_required(VERSION 3.20)
project(MaterialUIProject CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 是否启用 Skia（默认 ON，找不到就自动降级）
option(MATERIALUI_USE_SKIA "Build MaterialUI with Skia backend" ON)
# 允许用户手动指定 Skia 根目录
set(MATERIALUI_SKIA_ROOT "" CACHE PATH "Path to Skia installation root")

add_subdirectory(MaterialUI)
add_subdirectory(demo)