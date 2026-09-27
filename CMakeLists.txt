cmake_minimum_required(VERSION 3.20)
project(MaterialUI CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SKIA_ROOT "$ENV{HOME}/skia")

# ---------- 平台源文件 & 平台链接库 ----------
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

    set(platform_specific_sources
        src/widget/widget_cocoa.mm
    )
    
    set(platform_link_libs
        "${SKIA_ROOT}/lib/libskia.a"
        "-framework Cocoa"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreServices"
        "-framework AppKit"
        "-framework Foundation"
        "-framework ImageIO"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework IOKit"
        "-framework Security"
        "-framework UniformTypeIdentifiers"
        "-framework Accelerate"
        "-lz"
    )
elseif(WIN32)
    set(platform_specific_sources
        src/widget/widget_win32.cpp
    )

    # 视 SKIA_ROOT 在 Windows 上的实际布局调整
    set(platform_link_libs
        "${SKIA_ROOT}/lib/skia.lib"   # 或 skia.dll 的导入库 skia.lib
        user32
        gdi32
        msimg32        # AlphaBlend / GradientFill
        ole32          # 备用：COM，Skia D3D 后端等可能用到
        shell32        # 备用：拖放、Shell 相关
    )

    target_compile_definitions(MaterialUI PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX                 # 防止 windows.h 的 min/max 宏污染 std::min/max
        WIN32_LEAN_AND_MEAN      # 减少 windows.h 包含范围，加快编译
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ---------- 可执行目标 ----------
add_executable(MaterialUI
    main.cpp
    src/widget/widget.cpp
    ${platform_specific_sources}
    src/component/base/baseControler.hpp
    src/component/base/types.h
    src/render/canvas.hpp
    src/render/skia_canvas.hpp
)

target_include_directories(MaterialUI PRIVATE
    "${SKIA_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(MaterialUI PRIVATE
    ${platform_link_libs}
)

# ---------- MSVC 编译选项（可选但推荐）----------
if(MSVC)
    target_compile_options(MaterialUI PRIVATE
        /utf-8          # 源码按 UTF-8 解析，避免中文注释/字符串乱码
        /W4             # 提高警告等级
        /permissive-    # 更严格的标准一致性
        /Zc:__cplusplus # __cplusplus 宏返回正确值
    )
    set_property(TARGET MaterialUI PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()