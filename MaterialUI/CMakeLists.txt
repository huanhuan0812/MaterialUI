# ============================================================
#  MaterialUI 静态库
# ============================================================
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

elseif(UNIX AND NOT APPLE)
    pkg_check_modules(X11     REQUIRED x11 xext xrender)
    pkg_check_modules(CAIRO   REQUIRED cairo)
    pkg_check_modules(WAYLAND REQUIRED wayland-client)
    pkg_check_modules(XKB     REQUIRED xkbcommon)

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
        COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML} ${XDG_PROTO_H}
        COMMAND ${WAYLAND_SCANNER} private-code  ${XDG_SHELL_XML} ${XDG_PROTO_C}
        DEPENDS ${XDG_SHELL_XML}
        COMMENT "Generating xdg-shell protocol sources"
        VERBATIM
    )

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

# ---------- 库目标（原来是 add_executable）----------
add_library(MaterialUI STATIC
    src/native/NativeWidget/NativeWidget.cpp
    ${platform_specific_sources}
    src/component/base/baseControler.hpp
    src/core/types.h
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
    src/core/handlePool.hpp
)

# ---------- 头文件 & 包含目录（给库自己也给 demo 用 → PUBLIC）----------
target_include_directories(MaterialUI PUBLIC
    "${SKIA_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ---------- 平台专有 include / 依赖 ----------
if(APPLE)
    # 无需额外 include
elseif(WIN32)
    target_compile_definitions(MaterialUI PUBLIC
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    target_include_directories(MaterialUI PUBLIC
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        ${WAYLAND_INCLUDE_DIRS}
        ${XKB_INCLUDE_DIRS}
        "${CMAKE_CURRENT_BINARY_DIR}"      # 找到生成的 xdg-shell-client-protocol.h
    )

    # 把生成的协议源加进库
    target_sources(MaterialUI PRIVATE ${XDG_PROTO_C})
    set_source_files_properties(${XDG_PROTO_C} PROPERTIES
        GENERATED TRUE
        LANGUAGE C
    )
    set_source_files_properties(${XDG_PROTO_H} PROPERTIES GENERATED TRUE)

    # 自定义命令的输出文件必须由某个 target 使用才会被触发编译
    add_custom_target(xdg_shell_gen DEPENDS ${XDG_PROTO_H} ${XDG_PROTO_C})
    add_dependencies(MaterialUI xdg_shell_gen)
endif()

# ---------- 链接平台库（PUBLIC，因为 demo 也要链接系统框架）----------
target_link_libraries(MaterialUI PUBLIC
    ${platform_link_libs}
)

# ---------- MSVC ----------
if(MSVC)
    target_compile_options(MaterialUI PRIVATE
        /utf-8 /W4 /permissive- /Zc:__cplusplus
    )
    set_property(TARGET MaterialUI PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ---------- GCC/Clang ----------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MaterialUI PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()