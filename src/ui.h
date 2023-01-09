#pragma once

#include "util.h"
#include "draw.h"

#include <string_view>
#include <string>
#include <nanovg.h>
#include <vector>
#include <functional>
#include <span>
#include <robin_hood.h>
#include <any>

struct ui_key final {
    static ui_key root() {
        return ui_key{0}.next(0);
    }

    static ui_key null() {
        return {};
    }

    bool is_null() const {
        return m_null;
    }

    template <typename... Args>
    ui_key next(Args&&... arg) const {
        size_t hash = 0;
        hash_combine(hash, m_hash, std::forward<Args>(arg)...);
        return {hash};
    }

    uint64 value() const {
        sb_ASSERT(!is_null());
        return m_hash;
    }

    bool operator==(const ui_key& rhs) const {
        return !(is_null() != rhs.is_null() || value() != rhs.value());
    }

    bool operator!=(const ui_key& rhs) const {
        return !(*this == rhs);
    }

  private:
    ui_key(uint64 v) : m_null{false}, m_hash{v} {
    }

    ui_key() : m_null{true}, m_hash{UINT64_MAX} {
    }

    bool m_null;
    uint64 m_hash;
};

class space;
struct input_state;
struct ui_options;
struct ui_colors;
struct ui_state;
struct ui_layout;

template <typename Ret = void>
struct ui_view {
    virtual Ret view(ui_state& state, ui_layout& layout) = 0;
};

struct ui_memory final {
    template <typename T>
    T& state(ui_key key) {
        static_assert(std::is_default_constructible_v<T>);
        if (!m_state.contains(key.value())) {
            m_state.emplace(key.value(), T{});
        }
        return std::any_cast<T&>(m_state.at(key.value()));
    }

  private:
    robin_hood::unordered_flat_map<uint64, std::any> m_state;
};

struct ui_state final {
    draw_list* draw;
    input_state* input;
    ui_memory* memory;
    ui_key* focus;
    bool* focus_taken;

    const ui_options* opts;
    const ui_colors* colors;

    ui_key key = ui_key::root();
    uint64 next_ikey = 0;

    void take_focus(ui_key key) {
        *focus = key;
        *focus_taken = true;
    }

    template <typename... Args>
    ui_state with_next_key(Args&&... arg) {
        ui_state cpy = *this;
        cpy.key = key.next(next_ikey, std::forward<Args>(arg)...);
        cpy.next_ikey = 0;
        ++next_ikey;
        return cpy;
    }
};

struct ui_layout {
    virtual vector2f32 last_size() = 0;
    virtual void allocate_self(ui_layout& layout) = 0;
    virtual rect2f32 allocate(vector2f32 size) = 0;
};

struct ui_vstack final : ui_layout {
    ui_vstack(ui_state& state);
    
    vector2f32 last_size() override;
    void allocate_self(ui_layout& layout) override;
    rect2f32 allocate(vector2f32 size) override;

    float32 spacing = 0.f;
    rect2f32 rect;

  private:
    struct self_state final {
        vector2f32 sized;
    };

    self_state* m_state;
};

struct ui_button final : ui_view<bool> {
    ui_button(ui_state& state);
    bool view(ui_state& state, ui_layout& layout) override;

    NVGcolor text_color;
    NVGcolor tint;
    float32 tint_strength;
    draw_font font;
    float32 size;
    std::string text;
};

struct ui_dropdown_box final : ui_view<> {
    void view(ui_state& state, ui_layout& layout) override;

    uint32* index = nullptr;
    std::vector<std::string> options;
};
