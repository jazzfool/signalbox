#pragma once

#include "util.h"

#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <spdlog/fmt/fmt.h>

// very barebones linear allocator
struct alignas(64) Linear_Allocator final {
    static constexpr std::size_t MAX_BLOCKS = 64;

    struct Block_Header final {
        uint8* start;
        uint8* ptr;
        uint8* end;
    };

    Linear_Allocator(std::size_t sz) noexcept {
        blocks = (Block_Header*)malloc(MAX_BLOCKS * sizeof(Block_Header));
        block_count = 0;
        block_cap = MAX_BLOCKS;
        block_size = sz;
        max = 0;

        alloc_block();
    }

    ~Linear_Allocator() {
        free(blocks);
    }

    uint8* allocate(std::size_t n) noexcept {
        max += n;
        for (std::size_t i = 0; i < block_count; ++i) {
            const auto p = alloc_from_block(&blocks[i], n);
            if (p) {
                return p;
            }
        }
        block_size = n > block_size ? n * 4 : block_size;
        const auto block = alloc_block();
        if (!block) {
            return nullptr;
        }
        return alloc_from_block(block, n);
    }

    void deallocate() noexcept {
        max = 0;
        for (std::size_t i = 0; i < block_count; ++i) {
            blocks[i].ptr = blocks[i].start;
        }
    }

    std::size_t report_allocated() const noexcept {
        return max;
    }

  private:
    Block_Header* alloc_block() noexcept {
        if (block_count >= block_cap) {
            return nullptr;
        }
        const auto block = &blocks[block_count++];
        block->start = (uint8*)malloc(block_size);
        block->ptr = block->start;
        block->end = block->start + block_size;
        return block;
    }

    uint8* alloc_from_block(Block_Header* block, std::size_t n) noexcept {
        if (static_cast<std::size_t>(block->end - block->ptr) < n) {
            return nullptr;
        }
        const auto p = block->ptr;
        block->ptr += n;
        return p;
    }

    Block_Header* blocks;
    std::size_t block_count;
    std::size_t block_cap;
    std::size_t block_size;
    std::size_t max;
};

template <typename T>
struct Linear_Allocator_Typed final {
    using value_type = T;

    Linear_Allocator& alloc;

    Linear_Allocator_Typed(Linear_Allocator& alloc) : alloc{alloc} {
    }

    template <typename U>
    Linear_Allocator_Typed(const Linear_Allocator_Typed<U>& other) noexcept : alloc{other.alloc} {
    }

    [[nodiscard]] T* allocate(std::size_t n) {
        const auto p = alloc.allocate(n * sizeof(T));
        if (!p) {
            throw std::bad_alloc();
        }
        return reinterpret_cast<T*>(p);
    }

    void deallocate(T*, std::size_t) noexcept { /* no-op */
    }
};

template <typename T>
using Linear_Basic_String = std::basic_string<T, std::char_traits<T>, Linear_Allocator_Typed<T>>;

using Linear_String = Linear_Basic_String<char>;

template <typename T>
using Linear_Vector = std::vector<T, Linear_Allocator_Typed<T>>;

using linear_fmt_buffer =
    fmt::basic_memory_buffer<char, fmt::inline_buffer_size, Linear_Allocator_Typed<char>>;

inline Linear_String
linear_vformat(const Linear_Allocator_Typed<char>& alloc, fmt::string_view str, fmt::format_args args) {
    linear_fmt_buffer buf{alloc};
    fmt::vformat_to(std::back_inserter(buf), str, args);
    return Linear_String{buf.data(), buf.size(), alloc};
}

template <typename... Args>
inline Linear_String
linear_format(const Linear_Allocator_Typed<char>& alloc, fmt::string_view str, const Args&... args) {
    return linear_vformat(alloc, str, fmt::make_format_args(args...));
}
