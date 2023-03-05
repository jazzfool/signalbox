#pragma once

#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <spdlog/fmt/fmt.h>
#include <memory_resource>

using Pmr_Fmt_Memory =
    fmt::basic_memory_buffer<char, fmt::inline_buffer_size, std::pmr::polymorphic_allocator<char>>;

inline std::pmr::string
pmr_vformat(std::pmr::polymorphic_allocator<char> alloc, fmt::string_view str, fmt::format_args args) {
    Pmr_Fmt_Memory buf{alloc};
    fmt::vformat_to(std::back_inserter(buf), str, args);
    return std::pmr::string{buf.data(), buf.size(), alloc};
}

template <typename... Args>
inline std::pmr::string
pmr_format(const std::pmr::polymorphic_allocator<char>& alloc, fmt::string_view str, const Args&... args) {
    return pmr_vformat(alloc, str, fmt::make_format_args(args...));
}

class Pmr_Deleter {
  public:
    Pmr_Deleter(const std::pmr::polymorphic_allocator<std::byte>& alloc) : m_alloc{alloc} {
    }

    template <typename T>
    void operator()(T* p) {
        std::destroy_at(p);
        std::pmr::polymorphic_allocator<T>{m_alloc}.deallocate(p, 1);
    }

  private:
    std::pmr::polymorphic_allocator<std::byte> m_alloc;
};

template<typename T>
using Pmr_Unique_Ptr = std::unique_ptr<T, Pmr_Deleter>;

template<typename T>
Pmr_Unique_Ptr<T> pmr_null_unique(const std::pmr::polymorphic_allocator<T>& alloc) {
  return {nullptr, {alloc}};
}

template <typename T, typename... Args>
Pmr_Unique_Ptr<T> pmr_make_unique(std::pmr::polymorphic_allocator<T> alloc, Args&&... arg) {
    Pmr_Unique_Ptr<T> up{nullptr, {alloc}};
    T* p = alloc.template new_object<T>(std::forward<Args>(arg)...);
    up.reset(p);
    return up;
}
