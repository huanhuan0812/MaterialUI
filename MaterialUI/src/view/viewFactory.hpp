// src/render/view_factory.hpp
#pragma once

#include "backend.hpp"
#include <memory>

namespace ui {

class View;

class ViewFactory {
public:
    // 自动求解（无 hint）
    static std::unique_ptr<View> create(const BackendRequirement& req,
                                        int w, int h,
                                        SelectionInfo* out = nullptr);

    // 带 hint 求解（hint 在可用集合内则优先）
    static std::unique_ptr<View> createWith(const BackendRequirement& req,
                                            BackendId hint,
                                            int w, int h,
                                            SelectionInfo* out = nullptr);

    // 强制指定（调试用，绕过自动；仍受能力约束）
    static std::unique_ptr<View> createForced(BackendId id, int w, int h);

    // 纯查询：给定需求会选哪个（不创建）
    static BackendId resolve(const BackendRequirement& req);
    static BackendId resolveWith(const BackendRequirement& req, BackendId hint);

    // 可用性
    static bool isUsable(BackendId id, const BackendRequirement& req);
};

} // namespace ui
