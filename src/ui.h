#pragma once

#include "util.h"
#include "draw.h"
#include "state.h"

#include <string_view>
#include <string>
#include <nanovg.h>
#include <vector>
#include <functional>
#include <span>
#include <robin_hood.h>
#include <any>
#include <deque>
#include <spdlog/fmt/fmt.h>

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

    template <typename T, typename OrInsert>
    T& state(ui_key key, OrInsert&& or_insert) {
        static_assert(std::is_default_constructible_v<T>);
        if (!m_state.contains(key.value())) {
            m_state.emplace(key.value(), or_insert());
        }
        return std::any_cast<T&>(m_state.at(key.value()));
    }

    template <typename... Args>
    ui_key peek_key(Args&&... arg) {
        return m_key_stack.back().next(m_next_ikey++, std::forward<Args>(arg)...);
    }

    template <typename... Args>
    ui_key push_key(Args&&... arg) {
        const auto key = peek_key(std::forward<Args>(arg)...);
        m_key_stack.push_back(key);
        return key;
    }

    void push_existing_key(ui_key key) {
        m_key_stack.push_back(key);
    }

    template <typename... Args>
    void pop_key() {
        if (m_key_stack.size() > 1)
            m_key_stack.pop_back();
    }

  private:
    std::deque<ui_key> m_key_stack;
    uint64 m_next_ikey = 0;

    robin_hood::unordered_flat_map<uint64, std::any> m_state;
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

    bool has_focus(ui_key key) const {
        return !key.is_null() && key == *focus;
    }

  private:
    ui_state() {
    }
};

template <typename T>
T ui_default_or_insert() {
    return T{};
}

template <typename T, typename OrInsert = decltype(ui_default_or_insert<T>)>
T& ui_get_state(ui_key key, OrInsert&& or_insert = ui_default_or_insert<T>) {
    return ui_state::get().memory->state<T, OrInsert>(key, std::forward<OrInsert>(or_insert));
}

template <typename... Args>
inline ui_key ui_peek_key(Args&&... arg) {
    return ui_state::get().memory->peek_key(std::forward<Args>(arg)...);
}

template <typename... Args>
inline ui_key ui_push_key(Args&&... arg) {
    return ui_state::get().memory->push_key(std::forward<Args>(arg)...);
}

inline void ui_push_existing_key(ui_key key) {
    return ui_state::get().memory->push_existing_key(key);
}

inline void ui_pop_key() {
    return ui_state::get().memory->pop_key();
}

namespace ui {

struct widget_base {
    virtual ~widget_base() = default;
    virtual void operator()(const rect2f32& r) = 0;
};

template <typename F>
struct widget_functor final : widget_base {
    void operator()(const rect2f32& r) override {
        f(r);
    }

    explicit widget_functor(F&& f) : f{std::move(f)} {
    }

    F f;
};

template <typename F>
struct layout_functor final {
    layout_functor(F f) : f{f} {
    }

    layout_functor(const layout_functor&) = delete;
    layout_functor& operator=(const layout_functor&) = delete;

    layout_functor(layout_functor&&) = default;
    layout_functor& operator=(layout_functor&&) = default;

    layout_functor& operator()(auto&& child) {
        vector2f32 sz;
        auto c = std::move(child)(sz);
        cs.push_back(std::make_unique<widget_functor<decltype(c)>>(std::move(c)));
        szs.push_back(sz);
        return *this;
    }

    auto operator()(vector2f32& out_sz) {
        std::vector<rect2f32> rects;
        rects.reserve(szs.size());
        for (const auto& size : szs) {
            rects.push_back({{}, size});
        }
        return [f = f(szs, out_sz), rects = std::move(rects), cs = std::move(cs)](const rect2f32& r) mutable {
            f(rects, r);
            assert(rects.size() == cs.size());
            size_t i = 0;
            for (auto& child : cs) {
                (*child)(rects[i++]);
            }
        };
    }

    std::vector<vector2f32> szs;
    std::vector<std::unique_ptr<widget_base>> cs;
    F f;
};

auto layout(auto&& f) {
    return layout_functor{std::forward<decltype(f)>(f)};
}

template <typename Out>
const Out& grab(const Out& or_, const Out& x, const auto&...) {
    return x;
}

template <typename Out, typename First, typename = std::enable_if_t<!std::is_same_v<Out, First>>>
const Out& grab(const Out& or_, const First&, const auto&... rest) {
    return grab(or_, rest...);
}

template <typename Out, typename Last, typename = std::enable_if_t<!std::is_same_v<Out, Last>>>
const Out& grab(const Out& or_, const Last&) {
    return or_;
}

template <typename Out>
const Out& grab(const Out& or_) {
    return or_;
}

struct spacing final {
    float32 v;
};

struct font_size final {
    float32 v;
};

struct tint final {
    NVGcolor color;
    float32 strength;
};

struct sides final {
    float32 left;
    float32 right;
    float32 top;
    float32 bottom;

    static sides all(float32 x) {
        sides s;
        s.left = x;
        s.right = x;
        s.top = x;
        s.bottom = x;
        return s;
    }

    static sides xy(vector2f32 v) {
        sides s;
        s.left = v.x;
        s.right = v.x;
        s.top = v.y;
        s.bottom = v.y;
        return s;
    }

    vector2f32 offset() const {
        return {left, top};
    }

    vector2f32 sum() const {
        return {left + right, top + bottom};
    }
};

static constexpr auto evalsize = [](auto&& f) {
    vector2f32 sz;
    std::move(f)(sz);
    return sz;
};

static constexpr auto keyed = [](ui_key key, auto&& f) {
    return [key, f = std::move(f)](vector2f32& sz) mutable {
        ui_push_existing_key(key);
        auto rf = std::move(f)(sz);
        ui_pop_key();
        return [rf = std::move(rf)](const rect2f32& r) mutable { rf(r); };
    };
};

static constexpr auto vstack = [](auto... options) {
    const auto space = grab<spacing>({0.f}, options...).v;
    return layout([=](const std::vector<vector2f32>& cs, vector2f32& sz) {
        ui_push_key("vstack");

        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x = std::max(sz.x, c.x);
            sz.y += c.y + space;
        }
        sz.y = std::max(sz.y - space, 0.f);

        ui_pop_key();

        return [=](std::vector<rect2f32>& cs, const rect2f32& r) {
            auto pos = r.pos;
            for (auto& c : cs) {
                c.pos = pos;
                pos.y += c.size.y + space;
            }
            ui_pop_key();
        };
    });
};

static constexpr auto hstack = [](auto... options) {
    const auto space = grab<spacing>({0.f}, options...).v;
    return layout([=](const std::vector<vector2f32>& cs, vector2f32& sz) {
        ui_push_key("hstack");

        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x += c.x + space;
            sz.y = std::max(sz.y, c.y);
        }
        sz.x = std::max(sz.x - space, 0.f);

        ui_pop_key();

        return [=](std::vector<rect2f32>& cs, const rect2f32& r) {
            auto pos = r.pos;
            for (auto& c : cs) {
                c.pos = pos;
                pos.x += c.size.x + space;
            }
        };
    });
};

static constexpr auto padding = [](auto... options) {
    const auto sides_ = grab<sides>(sides::all(0.f), options...);
    return layout([=](const std::vector<vector2f32>& cs, vector2f32& sz) {
        ui_push_key("padding");
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz = glm::max(sz, c);
        }
        sz += sides_.sum();
        ui_pop_key();
        return [=](std::vector<rect2f32>& cs, const rect2f32& r) {
            const auto maxsz = r.size - sides_.sum();
            for (auto& c : cs) {
                c.pos = r.pos + sides_.offset();
                c.size = glm::min(c.size, maxsz);
            }
        };
    });
};

static constexpr auto hside = [](auto&& left, auto&& right) {
    return [left = std::move(left), right = std::move(right)](auto... options) {
        const auto spacing_ = grab<spacing>({0.f}, options...).v;
        return [spacing_, left = std::move(left), right = std::move(right)](vector2f32& sz) {
            ui_push_key("hside");
            vector2f32 leftsz, rightsz;
            auto rleft = std::move(left)(leftsz);
            auto rright = std::move(right)(rightsz);
            sz.x = leftsz.x + rightsz.x + spacing_;
            sz.y = std::max(leftsz.y, rightsz.y);
            ui_pop_key();
            return [=, rleft = std::move(rleft), rright = std::move(rright)](const rect2f32& r) {
                // give right width priority
                const auto rwidth = std::min(r.size.x, rightsz.x + spacing_);
                const auto lwidth = r.size.x - rwidth;
                rleft(rect2f32{r.pos, {lwidth, r.size.y}});
                rright(rect2f32{{r.max().x - rwidth + spacing_, r.pos.y}, {rwidth, r.size.y}});
            };
        };
    };
};

static constexpr auto minsize = [](auto&& inner) {
    return [inner = std::move(inner)](auto... options) {
        const auto minsz = grab<vector2f32>({0.f, 0.f}, options...);
        return [minsz, inner = std::move(inner)](vector2f32& sz) {
            ui_push_key("minsize");
            auto rinner = std::move(inner)(sz);
            sz = glm::max(sz, minsz);
            ui_pop_key();
            return [rinner = std::move(rinner)](const rect2f32& r) { rinner(r); };
        };
    };
};

static constexpr auto text = []<typename... Formats>(fmt::format_string<Formats...> fstr, Formats&&... fmts) {
    const auto s = fmt::format(fstr, std::forward<Formats>(fmts)...);
    return [s = std::move(s)](auto... options) {
        auto& state = ui_state::get();
        const auto color = grab<NVGcolor>(state.colors->fg, options...);
        const auto font = grab<draw_font>(draw_font::sans, options...);
        const auto size = grab<font_size>({state.opts->font_size}, options...).v;
        const auto align = grab<text_align>(text_align::left_middle, options...);
        return [=, s = std::move(s)](vector2f32& sz) {
            sz = state.draw->measure_text(s, font, size);
            return [=, s = std::move(s)](const rect2f32& r) {
                const auto pos = align == text_align::center_middle
                                     ? (r.min() + r.max()) / 2.f
                                     : vector2f32{r.min().x, (r.min().y + r.max().y) / 2.f};
                state.draw->text(pos, align, s, color, font, size);
            };
        };
    };
};

static constexpr auto button = [](auto&& inner, auto onclick) {
    return [inner = std::move(inner), onclick = std::move(onclick)](auto... options) {
        const auto tint_ = grab<tint>({nvgRGB(0, 0, 0), 0.f}, options...);
        auto padded = padding(sides::xy(ui_state::get().opts->text_pad));
        padded(std::move(inner));
        return [=, padded = std::move(padded), onclick = std::move(onclick)](vector2f32& sz) mutable {
            ui_push_key("button");
            auto rpadded = std::move(padded)(sz);
            ui_pop_key();
            return [=, rpadded = std::move(rpadded),
                    onclick = std::move(onclick)](const rect2f32& r) mutable {
                auto& state = ui_state::get();

                const auto hover = state.input->take_hover(r);
                const auto pressed = hover && state.input->mouse_is_pressed[0];
                const auto clicked = hover && state.input->mouse_just_pressed[0];

                auto from = blend_color(
                    state.colors->button_from, state.colors->button_to, pressed ? 1.f : (hover ? 0.5f : 0.f));
                auto to = state.colors->button_to;

                from = blend_color(from, tint_.color, tint_.strength);
                to = blend_color(to, tint_.color, tint_.strength);

                state.draw->tb_grad_fill_rrect(r, state.opts->corner_radius, from, to);

                state.draw->stroke_rrect(
                    r.half_round(), state.opts->corner_radius, state.colors->button_border,
                    state.opts->border_width);

                rpadded(r);

                if (clicked)
                    onclick();
            };
        };
    };
};

static constexpr auto dropdown = [](std::span<const std::string> enums, uint32& index) {
    return [=, &index](auto... options) {
        float32 minwidth = 0.f;
        for (const auto& e : enums) {
            const auto textsz = evalsize(text("{}", e)());
            minwidth = std::max(minwidth, textsz.x);
        }

        const auto key = ui_peek_key("dropdown");

        if (ui_state::get().has_focus(key)) {
            ui_state::get().draw->push_layer();
            auto vs = vstack();
            for (size_t i = 0; i < enums.size(); ++i) {
                vs(button(text("{}", enums[i])(), [i, &index]() { index = i; })());
            }
            vector2f32 sz;
            vs(sz)(rect2f32{{20.f, 20.f}, sz});
            ui_state::get().draw->pop_layer();
        }

        return keyed(
            key, button(
                     hside(minsize(text("{}", enums[index])())(vector2f32{minwidth, 0.f}), text("⯆")())(
                         spacing{5.f}),
                     [=] { ui_state::get().take_focus(key); })());
    };
};

} // namespace ui
