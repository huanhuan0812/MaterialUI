#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace ui {

using Handle = uint32_t;
constexpr Handle kInvalidHandle = 0;

template <typename T>
class HandlePool {
public:
    Handle add(std::shared_ptr<T> obj) {
        Handle h = next_++;
        map_[h] = std::move(obj);
        return h;
    }
    void remove(Handle h) { map_.erase(h); }
    std::shared_ptr<T> get(Handle h) const {
        auto it = map_.find(h);
        return it == map_.end() ? nullptr : it->second;
    }
    size_t size() const { return map_.size(); }

private:
    std::unordered_map<Handle, std::shared_ptr<T>> map_;
    Handle next_ = 1;
};

} // namespace ui