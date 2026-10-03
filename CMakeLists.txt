cmake_minimum_required(VERSION 3.20)
project(MaterialUIProject CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(MaterialUI)
add_subdirectory(demo)