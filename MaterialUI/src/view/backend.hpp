// src/view/backend.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace ui {

// ============================================================
// 后端 ID（可扩展，非枚举）
// ============================================================
enum class BackendId : uint32_t {
    Unknown = 0,

    // CPU 保底
    CpuSoftRaster = 1,

    // GPU（RHI 之上的具体后端）
    GpuMetal   = 10,
    GpuVulkan  = 11,
    GpuD3D11   = 12,
    GpuD3D12   = 13,
    GpuOpenGL  = 14,
    GpuGLES    = 15,

    // 原生层
    NativeCocoaLayer = 20,
    NativeD2D        = 21,
    NativeGdi        = 22,

    // 外部
    External = 30,
};

inline const char* toString(BackendId id) {
    switch (id) {
        case BackendId::Unknown:          return "Unknown";
        case BackendId::CpuSoftRaster:    return "CPU";
        case BackendId::GpuMetal:         return "Metal";
        case BackendId::GpuVulkan:        return "Vulkan";
        case BackendId::GpuD3D11:         return "D3D11";
        case BackendId::GpuD3D12:         return "D3D12";
        case BackendId::GpuOpenGL:        return "OpenGL";
        case BackendId::GpuGLES:          return "GLES";
        case BackendId::NativeCocoaLayer: return "CocoaLayer";
        case BackendId::NativeD2D:        return "D2D";
        case BackendId::NativeGdi:        return "GDI";
        case BackendId::External:         return "External";
    }
    return "?";
}

// ============================================================
// 能力位
// ============================================================
enum class Capability : uint64_t {
    None               = 0,

    // 基础
    CreateSurface      = 1ull << 0,   // 能创建
    RenderCorrect      = 1ull << 1,   // 渲染结果正确（无已知问题）
    HardwareAccel      = 1ull << 2,   // 硬件加速
    SharedMemory       = 1ull << 3,   // 零拷贝共享内存

    // 特性
    AlphaBlend         = 1ull << 8,
    PremultipliedAlpha = 1ull << 9,
    Srgb               = 1ull << 10,
    Msaa               = 1ull << 11,
    Stencil            = 1ull << 12,
    FloatTexture       = 1ull << 13,
    NpotTexture        = 1ull << 14,
    YuvSampling        = 1ull << 15,
    Compute            = 1ull << 16,

    // 性能档位
    FastPath           = 1ull << 24,
    SlowPath           = 1ull << 25,
    KnownBuggy         = 1ull << 26,
};

inline uint64_t operator|(Capability a, Capability b) {
    return static_cast<uint64_t>(a) | static_cast<uint64_t>(b);
}
inline uint64_t operator|(uint64_t a, Capability b) {
    return a | static_cast<uint64_t>(b);
}

// ============================================================
// 后端信息
// ============================================================
struct BackendInfo {
    BackendId   id       = BackendId::Unknown;
    std::string name;              // "Metal", "Vulkan (NVIDIA)", ...
    uint64_t    caps     = 0;      // Capability 位
    int         priority = 0;      // 越大越优先

    bool has(Capability c) const {
        return (caps & static_cast<uint64_t>(c)) != 0;
    }
    bool valid() const { return id != BackendId::Unknown; }
};

// ============================================================
// 后端需求（Widget 声明）
// ============================================================
struct BackendRequirement {
    uint64_t required          = 0;   // 必需能力（全部满足）
    uint64_t preferred         = 0;   // 期望能力（加分项）
    bool     allowCpuFallback  = true;
    int      priority          = 0;
};

// ============================================================
// 选择结果
// ============================================================
enum class BackendSelectionResult {
    Selected,               // 成功
    FallbackDueToMissing,   // 缺能力，已降级
    FallbackDueToExcluded,  // 被排除，已降级
    FallbackDueToFailure,   // 创建失败，已降级
    NoBackendAvailable,     // 全不可用
};

struct SelectionInfo {
    BackendSelectionResult result = BackendSelectionResult::NoBackendAvailable;
    BackendId requested = BackendId::Unknown;   // 开发者要的
    BackendId actual    = BackendId::Unknown;   // 实际用的
    std::string detail;                         // 人类可读原因
};

// ============================================================
// 库 API（全部无副作用，除 probe）
// ============================================================

// 探测（一次）
void probeBackends();

// 查询
const BackendInfo*       backendInfo(BackendId id);
std::vector<BackendInfo> availableBackends();
bool                     isAvailable(BackendId id);
bool                     supports(BackendId id, Capability c);

// 开发者覆盖
void setPreferredBackend(BackendId id);
void clearPreferredBackend();
std::optional<BackendId> preferredBackend();

void excludeBackend(BackendId id);
void includeBackend(BackendId id);
bool isExcluded(BackendId id);

// 选择结果
const SelectionInfo& lastSelection();

// 重建
void rebuildAllViews(BackendId id);

} // namespace ui
