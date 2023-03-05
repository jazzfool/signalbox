#pragma once

#include "util.h"
#include "draw.h"
#include "state.h"
#include "linear.h"

#include <optional>
#include <string_view>
#include <string>
#include <nanovg.h>
#include <vector>
#include <functional>
#include <span>
#include <robin_hood.h>
#include <deque>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <any>
#include <glm/gtc/epsilon.hpp>

struct UI_Key final {
    static UI_Key root();
    static UI_Key null();

    bool is_null() const;

    template <typename... Args>
    UI_Key next(Args&&... arg) const {
        size_t hash = m_hash;
        hash_combine(hash, std::forward<Args>(arg)...);
        auto key = UI_Key{static_cast<uint64>(hash)};
        return key;
    }

    uint64 value() const;

    bool operator==(const UI_Key& rhs) const;
    bool operator!=(const UI_Key& rhs) const;

  private:
    explicit UI_Key(uint64 v);
    UI_Key();

    bool m_null;
    uint64 m_hash;
};

struct Input_State;
struct UI_Options;
struct UI_Colors;
struct UI_State;
struct UI_Layout;

struct UI_Memory final {
    void begin_frame();

    template <typename T, typename OrInsert>
    T* state(UI_Key key, OrInsert&& or_insert) {
        if (!m_state.contains(key.value())) {
            m_state.emplace(key.value(), or_insert());
        }
        return std::any_cast<T>(&m_state.at(key.value()));
    }

    template <typename T, typename... Args, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
    T* scratch(Args&&... arg) {
        return new (m_scratch_mbr[m_scratch_idx].allocate(sizeof(T))) T(std::forward<Args>(arg)...);
    }

    template <typename... Args>
    UI_Key peek_key(Args&&... arg) {
        return m_key_stack.back().first.next(m_next_ikey++, std::forward<Args>(arg)...);
    }

    template <typename... Args>
    UI_Key push_key(Args&&... arg) {
        const auto key = peek_key(std::forward<Args>(arg)...);
        m_key_stack.emplace_back(key, m_next_ikey);
        m_next_ikey = 0;
        return key;
    }

    void push_existing_key(UI_Key key);

    template <typename... Args>
    void pop_key() {
        if (m_key_stack.size() > 1) {
            m_next_ikey = m_key_stack.back().second;
            m_key_stack.pop_back();
        }
    }

    template <typename T>
    std::pmr::polymorphic_allocator<T> mbr_alloc() {
        return {&m_mbr};
    }

  private:
    std::deque<std::pair<UI_Key, uint64>> m_key_stack;
    uint64 m_next_ikey = 0;

    robin_hood::unordered_node_map<uint64, std::any> m_state;

    std::pmr::monotonic_buffer_resource m_mbr;
    std::array<std::pmr::monotonic_buffer_resource, 2> m_scratch_mbr;
    uint8 m_scratch_idx;
};

namespace ui {

struct Widget_Base {
    virtual ~Widget_Base() = default;
    virtual void operator()(const Rect2_F32& r) = 0;
};

template <typename F>
struct Widget_Functor final : Widget_Base {
    void operator()(const Rect2_F32& r) override {
        f(r);
    }

    explicit Widget_Functor(F&& f) : f{std::move(f)} {
    }

    F f;
};

} // namespace ui

struct UI_State final {
    UI_State(const UI_State&) = delete;
    UI_State& operator=(const UI_State&) = delete;

    Draw_List* draw = nullptr;

    Input_State input;
    UI_Memory memory;

    UI_Key hot = UI_Key::null();
    UI_Key prev_hot = UI_Key::null();
    bool hot_taken = false;
    UI_Key focus = UI_Key::null();
    UI_Key prev_focus = UI_Key::null();
    bool focus_taken = false;

    UI_Options opts = UI_Options::default_options();
    UI_Colors colors = UI_Colors::dark();

    std::vector<std::pair<std::unique_ptr<ui::Widget_Base>, Rect2_F32>> overlays;

    static UI_State& get();

    void begin_frame(Draw_List& out);
    void end_frame();

    void push_overlay(auto&& f, Vector2_F32 pos) {
        Vector2_F32 sz;
        auto rf = std::move(f)(sz);
        overlays.emplace_back(
            std::make_unique<ui::Widget_Functor<decltype(rf)>>(std::move(rf)), Rect2_F32{pos, sz});
    }

    void take_hot(UI_Key key);
    void take_focus(UI_Key key);
    bool is_hot(UI_Key key) const;
    bool has_focus(UI_Key key) const;

  private:
    UI_State() {
    }
};

template <typename T>
T ui_default_or_insert() {
    return T{};
}

template <typename T, typename OrInsert = decltype(ui_default_or_insert<T>)>
T* ui_get_state(UI_Key key, OrInsert&& or_insert = ui_default_or_insert<T>) {
    return UI_State::get().memory.state<T, OrInsert>(key, std::forward<OrInsert>(or_insert));
}

template <typename T, typename... Args>
T* ui_alloc_scratch(Args&&... arg) {
    return UI_State::get().memory.scratch<T, Args...>(std::forward<Args>(arg)...);
}

template <typename... Args>
inline UI_Key ui_peek_key(Args&&... arg) {
    return UI_State::get().memory.peek_key(std::forward<Args>(arg)...);
}

template <typename... Args>
inline UI_Key ui_push_key(Args&&... arg) {
    return UI_State::get().memory.push_key(std::forward<Args>(arg)...);
}

void ui_push_existing_key(UI_Key key);
void ui_pop_key();

template <typename T>
inline std::pmr::polymorphic_allocator<T> ui_mbr_alloc() {
    return UI_State::get().memory.mbr_alloc<T>();
}

namespace ui {
namespace {

template <typename F>
struct Layout_Functor final {
    Layout_Functor(F f)
        : szs{ui_mbr_alloc<Vector2_F32>()}, cs{ui_mbr_alloc<Pmr_Unique_Ptr<Widget_Base>>()}, f{f} {
    }

    Layout_Functor(const Layout_Functor&) = delete;
    Layout_Functor& operator=(const Layout_Functor&) = delete;

    Layout_Functor(Layout_Functor&& other) = default;
    Layout_Functor& operator=(Layout_Functor&& other) = default;

    Layout_Functor& operator()(auto&& child) {
        Vector2_F32 sz;

        auto c = std::move(child)(sz);
        auto wf = Widget_Functor{std::move(c)};
        cs.push_back(std::move(pmr_make_unique<decltype(wf)>(ui_mbr_alloc<decltype(c)>(), std::move(wf))));

        szs.push_back(sz);
        return *this;
    }

    auto operator()(Vector2_F32& out_sz) && {
        std::pmr::vector<Rect2_F32> rects{ui_mbr_alloc<Rect2_F32>()};
        rects.reserve(szs.size());
        for (const auto& size : szs) {
            rects.push_back({{}, size});
        }
        return
            [f = f(szs, out_sz), rects = std::move(rects), cs = std::move(cs)](const Rect2_F32& r) mutable {
                f(rects, r);
                assert(rects.size() == cs.size());
                size_t i = 0;
                for (auto& child : cs) {
                    const auto rect = rects[i++];
                    if (glm::epsilonEqual(rect.size.x, 0.f, std::numeric_limits<float32>::epsilon()) ||
                        glm::epsilonEqual(rect.size.y, 0.f, std::numeric_limits<float32>::epsilon()))
                        continue;
                    (*child)(rect);
                }
            };
    }

    std::pmr::vector<Vector2_F32> szs;
    std::pmr::vector<Pmr_Unique_Ptr<Widget_Base>> cs;
    F f;
};

auto layout(auto&& f) {
    return Layout_Functor{std::forward<decltype(f)>(f)};
}

template <typename... Args>
auto chain(auto&& f, Args&&... arg) {
    (f(std::forward<Args>(arg)), ...);
    return f;
}

template <typename T, typename Tag = decltype([] {})>
struct Value final {
    Value(T v) : v{v} {
    }

    T& operator*() noexcept {
        return v;
    }

    const T& operator*() const noexcept {
        return v;
    }

    T v;
};

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

using Spacing = Value<float32>;
using Font_Size = Value<float32>;

struct Tint final {
    NVGcolor color;
    float32 strength;
};

using Fill_Crossaxis = Value<bool>;

struct Sides final {
    float32 left;
    float32 right;
    float32 top;
    float32 bottom;

    static Sides all(float32 x) {
        Sides s;
        s.left = x;
        s.right = x;
        s.top = x;
        s.bottom = x;
        return s;
    }

    static Sides xy(Vector2_F32 v) {
        Sides s;
        s.left = v.x;
        s.right = v.x;
        s.top = v.y;
        s.bottom = v.y;
        return s;
    }

    Vector2_F32 offset() const {
        return {left, top};
    }

    Vector2_F32 sum() const {
        return {left + right, top + bottom};
    }
};

using Width = Value<float32>;

auto evalsize(auto&& f) {
    Vector2_F32 sz;
    std::move(f)(sz);
    return sz;
}

auto peeksize(Vector2_F32& outsz, auto&& f) {
    Vector2_F32 sz;
    auto rf = std::move(f)(sz);
    outsz = sz;
    return [sz_ = sz, rf = std::move(rf)](Vector2_F32& sz) mutable {
        sz = sz_;
        return [rf = std::move(rf)](const Rect2_F32& r) mutable { rf(r); };
    };
}

auto keyed(UI_Key key, auto&& f) {
    ui_push_existing_key(key);
    auto r = std::move(f)();
    ui_pop_key();
    return r;
}

auto unsized(auto&& f) {
    return [f = std::move(f)](const Rect2_F32& r) {
        Vector2_F32 sz;
        std::move(f)(sz)(r);
    };
}

auto drawn(const Vector2_F32& sz, auto&& f) {
    return [sz, f = std::move(f)](Vector2_F32& out_sz) {
        out_sz = sz;
        return [f = std::move(f)](const Rect2_F32& r) { f(r); };
    };
}

auto before(auto&& f, auto&& before) {
    return [f = std::move(f), before = std::move(before)](Vector2_F32& sz) mutable {
        auto rf = std::move(f)(sz);
        return [rf = std::move(rf), before = std::move(before)](const Rect2_F32& r) mutable {
            unsized(std::move(before))(r);
            std::move(rf)(r);
        };
    };
}

auto empty(Vector2_F32& sz) {
    sz = {0.f, 0.f};
    return [](const Rect2_F32&) {};
}

auto iff(bool c, auto&& inner) {
    return [c, inner = std::move(inner)](Vector2_F32& sz) mutable {
        std::optional<decltype(std::move(inner)(sz))> rinner;
        sz = {0.f, 0.f};
        if (c)
            rinner.emplace(std::move(inner)(sz));
        return [c, rinner = std::move(rinner)](const Rect2_F32& r) mutable {
            if (rinner)
                (*rinner)(r);
        };
    };
}

auto vstack(auto... options) {
    const auto space = *grab<Spacing>({0.f}, options...);
    const auto xfill = *grab<Fill_Crossaxis>({false}, options...);
    return layout([=](const std::pmr::vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x = std::max(sz.x, c.x);
            sz.y += c.y + space;
        }
        sz.y = std::max(sz.y - space, 0.f);
        return [=](std::pmr::vector<Rect2_F32>& cs, const Rect2_F32& r) {
            auto yspace = r.size.y;
            auto pos = r.pos;
            for (auto& c : cs) {
                c.size.y = std::min(c.size.y, yspace);
                const float32 height = c.size.y + space;
                c.pos = pos;
                yspace = std::max(yspace - height, 0.f);
                pos.y += height;
                if (xfill)
                    c.size.x = r.size.x;
                c.size.x = std::min(c.size.x, r.size.x);
            }
        };
    });
};

auto hstack(auto... options) {
    const auto space = *grab<Spacing>({0.f}, options...);
    const auto yfill = *grab<Fill_Crossaxis>({false}, options...);
    return layout([=](const std::pmr::vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x += c.x + space;
            sz.y = std::max(sz.y, c.y);
        }
        sz.x = std::max(sz.x - space, 0.f);
        return [=](std::pmr::vector<Rect2_F32>& cs, const Rect2_F32& r) {
            auto pos = r.pos;
            for (auto& c : cs) {
                c.pos = pos;
                pos.x += c.size.x + space;
                if (yfill)
                    c.size.y = r.size.y;
            }
        };
    });
};

auto zstack(auto... options) {
    return layout([=](const std::pmr::vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz = glm::max(sz, c);
        }
        return [=](std::pmr::vector<Rect2_F32>& cs, const Rect2_F32& r) {
            for (auto& c : cs) {
                c = r;
            }
        };
    });
}

auto padding(auto... options) {
    const auto sides_ = grab<Sides>(Sides::all(0.f), options...);
    return layout([sides_](const std::pmr::vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz = glm::max(sz, c);
        }
        sz += sides_.sum();
        return [sides_](std::pmr::vector<Rect2_F32>& cs, const Rect2_F32& r) {
            const auto maxsz = r.size - sides_.sum();
            for (auto& c : cs) {
                c.pos = r.pos + sides_.offset();
                c.size = glm::min(c.size, maxsz);
            }
        };
    });
}

auto hside(auto&& left, auto&& right) {
    return [left = std::move(left), right = std::move(right)](auto... options) mutable {
        const auto spacing_ = *grab<Spacing>({0.f}, options...);
        return [spacing_, left = std::move(left), right = std::move(right)](Vector2_F32& sz) mutable {
            Vector2_F32 leftsz, rightsz;
            auto rleft = std::move(left)(leftsz);
            auto rright = std::move(right)(rightsz);
            sz.x = leftsz.x + rightsz.x + spacing_;
            sz.y = std::max(leftsz.y, rightsz.y);
            return [=, rleft = std::move(rleft), rright = std::move(rright)](const Rect2_F32& r) mutable {
                // give right width priority
                const auto rwidth = std::min(r.size.x, rightsz.x + spacing_);
                const auto lwidth = r.size.x - rwidth;
                rleft(Rect2_F32{r.pos, {lwidth, r.size.y}});
                rright(Rect2_F32{{r.max().x - rwidth + spacing_, r.pos.y}, {rwidth, r.size.y}});
            };
        };
    };
}

auto minsize(Vector2_F32 minsz) {
    return [minsz](auto&& inner) mutable {
        return [minsz, inner = std::move(inner)](Vector2_F32& sz) mutable {
            auto rinner = std::move(inner)(sz);
            sz = glm::max(sz, minsz);
            return [rinner = std::move(rinner)](const Rect2_F32& r) mutable { std::move(rinner)(r); };
        };
    };
}

auto text(auto... options) {
    auto& state = UI_State::get();
    const auto color = grab<NVGcolor>(state.colors.fg, options...);
    const auto font = grab<Draw_Font>(Draw_Font::Sans, options...);
    const auto size = *grab<Font_Size>({state.opts.font_size}, options...);
    const auto align = grab<Text_Align>(Text_Align::Left_Middle, options...);
    return [=, &state](fmt::string_view fstr, const auto&... args) {
        // const auto s = pmr_format(ui_mbr_alloc<char>(), fstr, args...);
        const auto s = fmt::vformat(fstr, fmt::make_format_args(args...));
        return [=, &state, s = std::move(s)](Vector2_F32& sz) {
            sz = state.draw->measure_text(s, font, size);
            return [=, &state, s = std::move(s)](const Rect2_F32& r) {
                const auto pos = align == Text_Align::Center_Middle
                                     ? (r.min() + r.max()) / 2.f
                                     : Vector2_F32{r.min().x, (r.min().y + r.max().y) / 2.f};
                state.draw->text(pos, align, s, color, font, size);
            };
        };
    };
}

struct Interaction final {
    bool hover : 1 = false;
    bool click : 1 = false;
    bool press : 1 = false;
    bool focus : 1 = false;
    float32 scroll = 0.f;
};

auto mux(auto&&... fs) {
    return [... fs = std::move(fs)](auto&& x) { (fs(std::forward<decltype(x)>(x)), ...); };
}

auto set_it(auto* it) {
    return [it](const auto& in) { *it = in; };
}

auto onclick(auto&& f) {
    return [f = std::move(f)](const Interaction& itr) {
        if (itr.click)
            f();
    };
}

using Scrollable = Value<bool>;

auto interact(auto... options) {
    const auto scrollable = *grab(Scrollable{false}, options...);
    return [scrollable](auto&& cb) {
        return [scrollable, cb = std::move(cb)](auto&& inner) mutable {
            const auto key = ui_peek_key("interact");
            return [scrollable, key, cb = std::move(cb), inner = std::move(inner)](Vector2_F32& sz) mutable {
                auto rinner = std::move(inner)(sz);
                return [scrollable, key, cb = std::move(cb),
                        rinner = std::move(rinner)](const Rect2_F32& r) mutable {
                    auto& state = UI_State::get();

                    Interaction itr;

                    if (state.has_focus(key)) {
                        itr.focus = true;
                    }

                    if (state.is_hot(key)) {
                        itr.hover = true;
                        itr.press = state.input.mouse_is_pressed[0];
                        if (state.input.mouse_just_pressed[0]) {
                            state.take_focus(key);
                            itr.click = true;
                        }
                    }

                    if (state.input.hover(state.draw->clip_rect()) && state.input.hover(r)) {
                        state.take_hot(key);
                        if (scrollable)
                            itr.scroll = state.input.take_scroll();
                    }

                    std::move(cb)(itr);

                    rinner(r);

                    return itr;
                };
            };
        };
    };
}

auto button(auto&& inner, auto&& cb) {
    return [inner = std::move(inner), cb = std::move(cb)](auto... options) mutable {
        const auto key = ui_push_key(consthash("button"));
        struct S {
            Interaction itr;
        }* s = ui_get_state<S>(key);

        const auto tint_ = grab<Tint>({nvgRGB(0, 0, 0), 0.f}, options...);
        auto r = interact()([s, cb = std::move(cb)](Interaction itr) mutable {
            s->itr = itr;
            std::move(cb)(itr);
        })(
            before(
                padding(Sides::xy(UI_State::get().opts.text_pad))(std::move(inner)),
                drawn({0.f, 0.f}, [s, tint_](const Rect2_F32& r) {
                    auto& state = UI_State::get();

                    auto from = blend_color(
                        state.colors.button_from, state.colors.button_to,
                        s->itr.press ? 1.f : (s->itr.hover ? 0.5f : 0.f));
                    auto to = state.colors.button_to;

                    from = blend_color(from, tint_.color, tint_.strength);
                    to = blend_color(to, tint_.color, tint_.strength);

                    state.draw->tb_grad_fill_rrect(r, state.opts.corner_radius, from, to);

                    state.draw->stroke_rrect(
                        r.half_round(), state.opts.corner_radius, state.colors.button_border,
                        state.opts.border_width);
                })));

        ui_pop_key(); // button
        return r;
    };
}

auto dropdown(std::span<const std::string_view> enums, uint32& index) {
    return [=, &index](auto... options) {
        float32 minwidth = 0.f;
        for (const auto& e : enums) {
            const auto textsz = evalsize(text()("{}", e));
            minwidth = std::max(minwidth, textsz.x);
        }

        const auto key = ui_push_key(consthash("dropdown"));

        ui_push_key(consthash("dropdown_menu"));
        auto& state = UI_State::get();
        if (state.has_focus(key)) {
            auto vs = vstack(Fill_Crossaxis{true});
            for (size_t i = 0; i < enums.size(); ++i) {
                const auto itemkey = ui_push_key(consthash("item"), i);
                auto s = ui_get_state<Interaction>(itemkey);

                static constexpr auto menubutton = [](Interaction* s, size_t i, uint32& index,
                                                      std::string_view e) {
                    return interact()([s, i, &index](Interaction itr) {
                        *s = itr;
                        if (itr.click) {
                            index = i;
                        }
                    })(
                        before(
                            padding(Sides::xy(UI_State::get().opts.text_pad))(text()("{}", e)),
                            drawn({0.f, 0.f}, [&s](const Rect2_F32& r) {
                                auto& state = UI_State::get();
                                if (s->hover)
                                    state.draw->fill_rrect(
                                        r, state.opts.corner_radius, state.colors.highlight_bg);
                            })));
                };

                vs(menubutton(s, i, index, enums[i]));

                ui_pop_key(); // item, i
            }

            auto overlay = before(padding(Sides::all(2.f))(vs), drawn({}, [](const Rect2_F32& r) {
                                      auto& state = UI_State::get();
                                      state.draw->fill_rrect(r, state.opts.corner_radius, state.colors.bg);
                                      state.draw->stroke_rrect(
                                          r.half_round(), state.opts.corner_radius, state.colors.border, 1.f);
                                  }));

            state.push_overlay(std::move(overlay), {20.f, 20.f});
        }
        ui_pop_key(); // dropdown_menu

        auto r = button(
            hside(minsize(Vector2_F32{minwidth, 0.f})(text()("{}", enums[index])), text()("⯆"))(Spacing{5.f}),
            [=](Interaction itr) {
                if (itr.click)
                    UI_State::get().take_focus(key);
            })();

        ui_pop_key(); // dropdown
        return r;
    };
}

using Placeholder = Value<std::pmr::string>;

auto textbox(std::string& txt) {
    return [&txt](auto... options) {
        auto& state = UI_State::get();

        const auto placeholder_ = *grab<Placeholder>({ui_linear_str("")}, options...);
        const auto width_ = *grab<Width>({100.f}, options...);
        const auto font_size_ = *grab<Font_Size>({state.opts.font_size}, options...);

        const auto key = ui_push_key(consthash("textbox"));

        struct S {
            Interaction itr;
            uint32 cursor = 0;
        }* s = ui_get_state<S>(key);

        auto zs = chain(
            zstack(),
            //
            drawn(
                {},
                [&s, &state](const Rect2_F32& r) {
                    state.draw->fill_rrect(
                        r, state.opts.corner_radius,
                        s->itr.focus ? state.colors.input_focus_bg : state.colors.input_bg);

                    state.draw->stroke_rrect(
                        r.half_round(), state.opts.corner_radius,
                        s->itr.focus ? state.colors.focus_border : state.colors.border,
                        state.opts.border_width);
                }),
            //
            minsize(Vector2_F32{width_, 0.f})(interact()([&s, &state, &txt](Interaction itr) {
                s->itr = itr;
                if (itr.focus) {
                    if (s->cursor > 0 && state.input.keys_just_pressed[GLFW_KEY_LEFT]) {
                        --s->cursor;
                    }
                    if (s->cursor < txt.size() && state.input.keys_just_pressed[GLFW_KEY_RIGHT]) {
                        ++s->cursor;
                    }

                    if (state.input.text) {
                        txt.insert(txt.begin() + s->cursor++, *state.input.text);
                    }

                    if (s->cursor > 0 && state.input.keys_just_pressed[GLFW_KEY_BACKSPACE]) {
                        txt.erase(txt.begin() + --s->cursor);
                    }

                    if (state.input.keys_just_pressed[GLFW_KEY_ENTER]) {
                        state.take_focus(UI_Key::null());
                    }
                }
            })(padding(Sides::xy(state.opts.text_pad))(text(Draw_Font::Mono)("{}", txt)))),
            //
            iff(s->itr.focus, drawn({}, [=, &s, &state, &txt](const Rect2_F32& r) {
                    float32 cwidth = 0.f;
                    state.draw->measure_text("A", Draw_Font::Mono, font_size_, &cwidth);

                    state.draw->fill_rrect(
                        {r.pos + Vector2_F32{cwidth * s->cursor + state.opts.text_pad.x, 2.f},
                         {cwidth, r.size.y - 4.f}},
                        0.f, state.colors.fg);

                    if (s->cursor < txt.size()) {
                        state.draw->text(
                            r.pos + state.opts.text_pad + Vector2_F32{cwidth * s->cursor, font_size_ / 2.f},
                            Text_Align::Left_Middle, txt.substr(s->cursor, 1), state.colors.input_focus_bg,
                            Draw_Font::Mono, font_size_);
                    }
                })));

        ui_pop_key(); // textbox

        return zs;
    };
}

enum class Scroll_Direction { Vertical, Horizontal, Both };

auto scroll_view(auto... options) {
    const auto dir_ = grab(Scroll_Direction::Vertical, options...);
    return [=](auto&& inner) mutable {
        auto& state = UI_State::get();

        const auto key = ui_push_key(consthash("scroll_view"));

        struct S {
            float32 scroll = 0.f;
        }* s = ui_get_state<S>(key);

        const auto itr = ui_alloc_scratch<Interaction>();

        auto scroll_bar = [itr](float32 scroll, float32 length) {
            return interact()(set_it(itr))(
                drawn({UI_State::get().opts.scroll_bar_width, 0.f}, [=](const Rect2_F32& r) {
                    auto& state = UI_State::get();

                    const auto px_length = r.size.y * r.size.y / length;
                    const Rect2_F32 bar{
                        {r.pos.x, lerp(r.pos.y, r.max().y - px_length, scroll)}, {r.size.x, px_length}};

                    const auto hover = itr->hover && state.input.hover(bar);
                    const auto press = hover && itr->press;

                    state.draw->fill_rrect(r, r.size.x / 2.f, state.colors.scroll_bar_bg);
                    state.draw->fill_rrect(
                        bar, r.size.x / 2.f,
                        blend_color(
                            state.colors.scroll_bar_fg, state.colors.scroll_bar_press_fg,
                            press ? 1.f : (hover ? 0.5f : 0.f)));
                }));
        };

        Vector2_F32 inner_sz;
        auto rinner = std::move(inner)(inner_sz);
        auto content = [s, inner_sz, rinner = std::move(rinner)](Vector2_F32& sz) mutable {
            sz = inner_sz;
            return [s, inner_sz, rinner = std::move(rinner)](const Rect2_F32& r) mutable {
                auto& state = UI_State::get();
                auto r2 = r;
                r2.pos.y -= s->scroll * (inner_sz.y - r.size.y);
                r2.size = inner_sz;
                state.draw->push_clip_rect(r);
                rinner(r2);
                state.draw->pop_clip_rect();
            };
        };

        auto ui = interact(Scrollable{true})([inner_sz, s](Interaction itr) {
            s->scroll -= (30.f * itr.scroll) / inner_sz.y;
            s->scroll = clamp(0.f, 1.f, s->scroll);
        })(chain(hstack(Fill_Crossaxis{true}), std::move(content), scroll_bar(s->scroll, inner_sz.y)));

        ui_pop_key(); // scroll_view

        return ui;
    };
}

} // namespace
} // namespace ui