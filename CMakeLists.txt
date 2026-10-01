cmake_minimum_required(VERSION 3.20)
project(MaterialUI CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SKIA_ROOT "$ENV{HOME}/skia")

find_package(PkgConfig REQUIRED)

# ---------- 平台源文件 & 平台链接库 ----------
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

    set(platform_specific_sources
        src/native/NativeWidget/macNativeWidget.mm
        src/canvas/cocoaView.mm
        src/native/systemApi/macApi.mm
        src/native/event/cocoaEventLoop.mm
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
        src/native/NativeWidget/winNativeWidget.cpp
        src/canvas/win32View.cpp
        src/native/systemApi/winApi.cpp
        src/native/event/win32EventLoop.cpp
    )

    set(platform_link_libs
        "${SKIA_ROOT}/lib/skia.lib"
        user32
        gdi32
        msimg32
        ole32
        shell32
    )

    target_compile_definitions(MaterialUI PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

elseif(UNIX AND NOT APPLE)
    # ---------- Linux：X11 + Wayland 双后端 ----------
    pkg_check_modules(X11     REQUIRED x11 xext xrender)
    pkg_check_modules(CAIRO   REQUIRED cairo)
    pkg_check_modules(WAYLAND REQUIRED wayland-client)
    pkg_check_modules(XKB     REQUIRED xkbcommon)   # 若暂时不做键盘字符解析，可去掉

    # 生成 xdg-shell 协议代码（放在构建目录里）
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")

    if(NOT EXISTS "${XDG_SHELL_XML}")
        message(FATAL_ERROR
            "xdg-shell.xml not found at ${XDG_SHELL_XML}. "
            "Install wayland-protocols.")
    endif()

    set(XDG_PROTO_H  "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
    set(XDG_PROTO_C  "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")

    add_custom_command(
        OUTPUT  ${XDG_PROTO_H} ${XDG_PROTO_C}
        COMMAND ${WAYLAND_SCANNER} client-header
                ${XDG_SHELL_XML} ${XDG_PROTO_H}
        COMMAND ${WAYLAND_SCANNER} private-code
                ${XDG_SHELL_XML} ${XDG_PROTO_C}
        DEPENDS ${XDG_SHELL_XML}
        COMMENT "Generating xdg-shell protocol sources"
        VERBATIM
    )

    # 因为 add_custom_command 的 OUTPUT 是在编译阶段才出现，
    # 把它做成一个 INTERFACE 库，方便 target_link_libraries 关联依赖。
    add_library(xdg_shell_proto INTERFACE)
    target_sources(xdg_shell_proto INTERFACE
        ${XDG_PROTO_C}
        ${XDG_PROTO_H}
    )
    set_source_files_properties(${XDG_PROTO_C} PROPERTIES
        GENERATED TRUE
        LANGUAGE C
    )
    set_source_files_properties(${XDG_PROTO_H} PROPERTIES GENERATED TRUE)

    set(platform_specific_sources
        src/native/NativeWidget/linuxX11NativeWidget.cpp
        src/native/NativeWidget/linuxWaylandNativeWidget.cpp
        src/native/event/x11EventLoop.cpp
        src/native/event/waylandEventLoop.cpp
    )

    set(platform_link_libs
        "${SKIA_ROOT}/lib/libskia.a"
        pthread
        dl
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        ${WAYLAND_LIBRARIES}
        ${XKB_LIBRARIES}
        fontconfig
        freetype
    )
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ---------- 可执行目标 ----------
add_executable(MaterialUI
    main.cpp
    src/native/NativeWidget/NativeWidget.cpp
    ${platform_specific_sources}
    src/component/base/baseControler.hpp
    src/component/base/types.h
    src/canvas/view.hpp
    src/canvas/skiaView.hpp
    src/canvas/skiaView.cpp
    src/component/input/Button.hpp
    src/app/application.hpp
    src/window/window.hpp
    src/native/event/EventLoop.hpp
    src/native/event/EventLoop.cpp
    src/window/window.cpp
    src/app/application.cpp
    src/native/systemApi/sysApi.hpp
    src/native/systemApi/sysApi.cpp
)

target_include_directories(MaterialUI PRIVATE
    "${SKIA_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# 平台专有 include（Linux / macOS / Windows 各自的 pkg-config 或 SDK 路径）
if(APPLE)
    # 暂时无需额外 include
elseif(WIN32)
    # SKIA_ROOT 已包含
elseif(UNIX AND NOT APPLE)
    target_include_directories(MaterialUI PRIVATE
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        ${WAYLAND_INCLUDE_DIRS}
        ${XKB_INCLUDE_DIRS}
        "${CMAKE_CURRENT_BINARY_DIR}"      # 找到生成的 xdg-shell-client-protocol.h
    )
    # 让生成文件成为编译依赖
    add_dependencies(MaterialUI xdg_shell_proto)
endif()

target_link_libraries(MaterialUI PRIVATE
    ${platform_link_libs}
)

# Linux 下把 xdg-shell 生成源加进可执行目标
if(UNIX AND NOT APPLE)
    target_sources(MaterialUI PRIVATE ${XDG_PROTO_C})
endif()

# ---------- MSVC 编译选项 ----------
if(MSVC)
    target_compile_options(MaterialUI PRIVATE
        /utf-8
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    set_property(TARGET MaterialUI PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ---------- GCC/Clang 下的通用告警（可选）----------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MaterialUI PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()