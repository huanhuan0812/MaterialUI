// src/core/handle_pool.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cassert>

using Handle = std::uint32_t;
inline constexpr Handle kInvalidHandle = 0;

template <typename T>
class HandlePool {
public:
    Handle add(std::shared_ptr<T> obj);
    void   remove(Handle h);

    std::shared_ptr<T> get(Handle h) const;
    bool   valid(Handle h) const;

    template <typename Fn>
    void forEach(Fn&& fn) const;

    std::size_t size() const;

private:
    std::unordered_map<Handle, std::shared_ptr<T>> map_;
    std::vector<Handle> order_;      // 可选：保持插入顺序
    Handle next_ = 1;                // 0 保留为 invalid
};