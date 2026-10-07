# ============================================================
#  MaterialUI 静态库
# ============================================================
find_package(PkgConfig REQUIRED)

# ---------- 查找 Skia（可选）----------
set(MATERIALUI_HAS_SKIA FALSE)

if(MATERIALUI_USE_SKIA)
    set(_skia_hints "")
    if(MATERIALUI_SKIA_ROOT)
        list(APPEND _skia_hints "${MATERIALUI_SKIA_ROOT}")
    endif()
    if(DEFINED ENV{SKIA_ROOT})
        list(APPEND _skia_hints "$ENV{SKIA_ROOT}")
    endif()
    list(APPEND _skia_hints "$ENV{HOME}/skia")

    find_path(SKIA_INCLUDE_DIR
        NAMES include/core/SkCanvas.h
        HINTS ${_skia_hints}
        PATH_SUFFIXES "" include
    )

    if(APPLE OR UNIX)
        find_library(SKIA_LIBRARY
            NAMES skia libskia.a
            HINTS ${_skia_hints}
            PATH_SUFFIXES lib lib64
        )
    elseif(WIN32)
        find_library(SKIA_LIBRARY
            NAMES skia skia.lib
            HINTS ${_skia_hints}
            PATH_SUFFIXES lib lib64 out/Release
        )
    endif()

    if(SKIA_INCLUDE_DIR AND SKIA_LIBRARY)
        set(MATERIALUI_HAS_SKIA TRUE)
        message(STATUS "MaterialUI: Skia found at ${SKIA_INCLUDE_DIR} / ${SKIA_LIBRARY}")
    else()
        message(STATUS "MaterialUI: Skia NOT found, building without Skia backend")
    endif()
endif()

# ---------- Skia 相关源文件（可选）----------
set(MATERIALUI_SKIA_SOURCES "")
if(MATERIALUI_HAS_SKIA)
    set(MATERIALUI_SKIA_SOURCES
        src/canvas/skiaCanvas.cpp
    )
endif()

# ---------- 平台源文件 & 平台链接库 ----------
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

    set(platform_specific_sources
        src/native/NativeWidget/macNativeWidget.mm
        src/canvas/cocoaCanvas.mm
        src/native/systemApi/macApi.mm
        src/native/event/cocoaEventLoop.mm
    )

    set(platform_link_libs
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
        src/canvas/win32Canvas.cpp
        src/native/systemApi/winApi.cpp
        src/native/event/win32EventLoop.cpp
    )

    set(platform_link_libs
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

# ---------- 如果启用了 Skia，把库追加到平台链接库 ----------
if(MATERIALUI_HAS_SKIA)
    list(APPEND platform_link_libs "${SKIA_LIBRARY}")
endif()

# ---------- 库目标 ----------
add_library(MaterialUI STATIC
    src/native/NativeWidget/NativeWidget.cpp
    ${platform_specific_sources}

    # ---- Skia 相关源文件（可选）----
    ${MATERIALUI_SKIA_SOURCES}

    src/core/types.h
    src/canvas/canvas.hpp
    src/canvas/skiaCanvas.hpp
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
    src/core/widgetId.hpp
    src/component/widget.hpp
    src/view/view.hpp
    src/view/cpuView.cpp
    src/view/backend.hpp
    src/view/viewFactory.hpp
)

# ---------- 头文件 & 包含目录 ----------
target_include_directories(MaterialUI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(MATERIALUI_HAS_SKIA)
    target_include_directories(MaterialUI PUBLIC "${SKIA_INCLUDE_DIR}")
    target_compile_definitions(MaterialUI PUBLIC MATERIALUI_HAS_SKIA=1)
endif()

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
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_sources(MaterialUI PRIVATE ${XDG_PROTO_C})
    set_source_files_properties(${XDG_PROTO_C} PROPERTIES
        GENERATED TRUE
        LANGUAGE C
    )
    set_source_files_properties(${XDG_PROTO_H} PROPERTIES GENERATED TRUE)

    add_custom_target(xdg_shell_gen DEPENDS ${XDG_PROTO_H} ${XDG_PROTO_C})
    add_dependencies(MaterialUI xdg_shell_gen)
endif()

# ---------- 链接平台库 ----------
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