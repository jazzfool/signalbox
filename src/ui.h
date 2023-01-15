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
#include <deque>

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

struct ui_memory final {
    void begin_frame() {
        m_key_stack.clear();
        m_key_stack.push_back(ui_key::root());
        m_next_ikey = 0;
    }

    template <typename T>
    T& state(ui_key key) {
        static_assert(std::is_default_constructible_v<T>);
        if (!m_state.contains(key.value())) {
            m_state.emplace(key.value(), T{});
        }
        return std::any_cast<T&>(m_state.at(key.value()));
    }

  private:
    friend struct ui_key_guard;

    std::deque<ui_key> m_key_stack;
    uint64 m_next_ikey = 0;

    robin_hood::unordered_flat_map<uint64, std::any> m_state;
};

struct ui_key_guard final {
    ui_key_guard(const ui_key_guard&) = delete;
    ui_key_guard& operator=(const ui_key_guard&) = delete;

    ~ui_key_guard();

    ui_key get() const {
        return m_key;
    }

    ui_key operator*() const {
        return m_key;
    }

  private:
    friend struct ui_state;

    template <typename... Args>
    ui_key_guard(ui_state& state, Args&&... args);

    ui_state* m_state;
    ui_key m_key;
};

struct ui_state final {
    draw_list* draw = nullptr;
    input_state* input = nullptr;
    ui_memory* memory = nullptr;
    ui_key* focus = nullptr;
    bool focus_taken = false;

    const ui_options* opts = nullptr;
    const ui_colors* colors = nullptr;

    std::deque<ui_layout*> layout_stack;

    static ui_state& get() {
        static ui_state s;
        return s;
    }

    void take_focus(ui_key key) {
        *focus = key;
        focus_taken = true;
    }

    template <typename... Args>
    ui_key_guard push_key(Args&&... arg) {
        return ui_key_guard{*this, std::forward<Args>(arg)...};
    }

    void push_layout(ui_layout& layout) {
        layout_stack.push_back(&layout);
    }

    void pop_layout() {
        layout_stack.pop_back();
    }

    ui_layout& active_layout() {
        return *layout_stack.back();
    }

  private:
    ui_state() {
    }
};

inline ui_key_guard::~ui_key_guard() {
    m_state->memory->m_key_stack.pop_back();
}

template <typename... Args>
ui_key_guard::ui_key_guard(ui_state& state, Args&&... arg) : m_state{&state}, m_key{ui_key::null()} {
    m_key =
        m_state->memory->m_key_stack.back().next(m_state->memory->m_next_ikey++, std::forward<Args>(arg)...);
    m_state->memory->m_key_stack.push_back(m_key);
}

template <typename T>
T& ui_get_state(ui_key key) {
    return ui_state::get().memory->state<T>(key);
}

struct ui_layout {
    ui_layout(std::string_view namekey);

    virtual vector2f32 min_size() final;
    virtual void allocate_self(ui_layout& layout) final;
    virtual rect2f32 allocate(vector2f32 size) final;
    virtual rect2f32 last_allocated() const final;

    virtual void begin() final;

    rect2f32 rect;

  protected:
    virtual ui_key key() const final;
    virtual vector2f32& get_min_size() final;
    virtual rect2f32 allocate_rect(vector2f32 size) = 0;

  private:
    struct self_state final {
        vector2f32 min_size;
    };

    ui_key_guard m_key;
    self_state* m_state;
    vector2f32 m_min_size;
    rect2f32 m_last;
};

template <typename Layout>
void ui_push_no_alloc_layout(Layout& layout) {
    layout.begin();
    ui_state::get().push_layout(layout);
}

template <typename Layout>
void ui_push_layout(Layout& layout) {
    layout.allocate_self(ui_state::get().active_layout());
    ui_push_no_alloc_layout<Layout>(layout);
}

inline void ui_pop_layout() {
    ui_state::get().pop_layout();
}

template <typename Layout, typename Ui>
void ui_with_layout(Layout& layout, Ui ui) {
    ui_push_layout(layout);
    ui();
    ui_pop_layout();
}

template <typename Layout, typename Ui>
void ui_with_no_alloc_layout(Layout& layout, Ui ui) {
    ui_push_no_alloc_layout(layout);
    ui();
    ui_pop_layout();
}

struct ui_exact final : ui_layout {
    ui_exact();

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;

  private:
    bool m_once;
};

struct ui_sized final : ui_layout {
    ui_sized();

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;
};

struct ui_fixed final : ui_layout {
    ui_fixed(vector2f32 size);

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;

  private:
    vector2f32 m_size;
};

struct ui_padding final : ui_layout {
    ui_padding();

    float32 left = 0.f;
    float32 right = 0.f;
    float32 top = 0.f;
    float32 bottom = 0.f;

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;

  private:
    bool m_once;
};

// lays out right-to-left
struct ui_row2 final : ui_layout {
    ui_row2();

    float32 spacing = 0.f;

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;

  private:
    uint32 m_i;
};

struct ui_vstack final : ui_layout {
    ui_vstack();

    float32 spacing = 0.f;

  protected:
    virtual rect2f32 allocate_rect(vector2f32 size) override;
};

struct ui_func {
    virtual void call() = 0;

    void operator()() {
        return this->call();
    }
};

template <typename F>
struct ui_lambda_func final : ui_func {
    F f;

    explicit ui_lambda_func(F&& f) : f{std::move(f)} {
    }

    void call() override {
        f();
    }
};

template <typename F>
ui_lambda_func<F> ui_lambda(F&& f) {
    return ui_lambda_func<F>{std::forward<F>(f)};
}

struct ui_text_opts final {
    ui_text_opts();

    static ui_text_opts with_text(std::string_view text);

    vector2f32 measure() const;

    std::string text;
    NVGcolor color;
    draw_font font;
    float32 font_size;
    text_align align;
};

void ui_text(const ui_text_opts& opts);

struct ui_button_opts final {
    ui_button_opts();

    NVGcolor tint;
    float32 tint_strength;
};

bool ui_button(const ui_button_opts& opts, ui_func&& inner);
bool ui_text_button(const ui_button_opts& opts, std::string_view text);

struct ui_dropdown_box_opts final {
    uint32* index = nullptr;
    std::span<const std::string> options;
};

void ui_dropdown_box(const ui_dropdown_box_opts& opts);

struct ui_menu_opts final {
    ui_menu_opts();

    float32 font_size;
    draw_font font;
    float32 spacing;
    uint32 current;
    std::span<const std::string> options;
};

uint32 ui_menu(const ui_menu_opts& opts);
