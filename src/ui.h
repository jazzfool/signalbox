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
struct ui_layout;

struct UI_Memory final {
    void begin_frame();

    template <typename T, typename OrInsert>
    T& state(UI_Key key, OrInsert&& or_insert) {
        static_assert(std::is_default_constructible_v<T>);
        if (!m_state.contains(key.value())) {
            m_state.emplace(key.value(), or_insert());
        }
        return std::any_cast<T&>(m_state.at(key.value()));
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
    Linear_Allocator_Typed<T> linear_alloc() {
        return {*m_linear};
    }

  private:
    std::deque<std::pair<UI_Key, uint64>> m_key_stack;
    uint64 m_next_ikey = 0;

    robin_hood::unordered_flat_map<uint64, std::any> m_state;

    std::optional<Linear_Allocator> m_linear;
    uint64 m_max_linear = 0;
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
T& ui_get_state(UI_Key key, OrInsert&& or_insert = ui_default_or_insert<T>) {
    return UI_State::get().memory.state<T, OrInsert>(key, std::forward<OrInsert>(or_insert));
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
inline Linear_Allocator_Typed<T> ui_linear_alloc() {
    return UI_State::get().memory.linear_alloc<T>();
}

Linear_String ui_linear_str(const char* s);

namespace ui {
namespace {

template <typename F>
struct Layout_Functor final {
    static constexpr std::size_t CHILD_FUNCTOR_SIZE = 512;

    Layout_Functor(F f)
        : szs{ui_linear_alloc<Vector2_F32>()}, cs{ui_linear_alloc<uint8[CHILD_FUNCTOR_SIZE]>()}, f{f} {
    }

    ~Layout_Functor() {
        for (auto& child : cs) {
            reinterpret_cast<Widget_Base*>(&child)->~Widget_Base();
        }
    }

    Layout_Functor(const Layout_Functor&) = delete;
    Layout_Functor& operator=(const Layout_Functor&) = delete;

    Layout_Functor(Layout_Functor&& other)
        : szs{std::move(other.szs)}, cs{std::move(other.cs)}, f(std::move(other.f)) {
    }

    Layout_Functor& operator=(Layout_Functor&& other) {
        szs = std::move(other.szs);
        cs = std::move(other.cs);
        f = std::move(other.f);
        return *this;
    }

    Layout_Functor& operator()(auto&& child) {
        Vector2_F32 sz;

        auto c = std::move(child)(sz);
        cs.emplace_back();
        Widget_Functor<decltype(c)> wfunc{std::move(c)};
        sb_ASSERT(sizeof(wfunc) <= CHILD_FUNCTOR_SIZE);
        new (&cs.back()) decltype(wfunc)(std::move(wfunc));

        szs.push_back(sz);
        return *this;
    }

    auto operator()(Vector2_F32& out_sz) {
        Linear_Vector<Rect2_F32> rects{ui_linear_alloc<Rect2_F32>()};
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
                    const auto pchild = reinterpret_cast<Widget_Base*>(&child);
                    (*pchild)(rects[i++]);
                }
            };
    }

    struct buf {
        uint8 m[CHILD_FUNCTOR_SIZE];
    };

    Linear_Vector<Vector2_F32> szs;
    Linear_Vector<buf> cs;
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
};

auto keyed(UI_Key key, auto&& f) {
    ui_push_existing_key(key);
    auto r = std::move(f)();
    ui_pop_key();
    return r;
};

auto unsized(auto&& f) {
    return [f = std::move(f)](const Rect2_F32& r) {
        Vector2_F32 sz;
        std::move(f)(sz)(r);
    };
};

auto drawn(const Vector2_F32& sz, auto&& f) {
    return [sz, f = std::move(f)](Vector2_F32& out_sz) {
        out_sz = sz;
        return [f = std::move(f)](const Rect2_F32& r) { f(r); };
    };
};

auto before(auto&& f, auto&& before) {
    return [f = std::move(f), before = std::move(before)](Vector2_F32& sz) mutable {
        auto rf = std::move(f)(sz);
        return [rf = std::move(rf), before = std::move(before)](const Rect2_F32& r) mutable {
            unsized(std::move(before))(r);
            std::move(rf)(r);
        };
    };
};

auto sandwich(auto&& f, auto&& before, auto&& after) {
    return [f = std::move(f), before = std::move(before), after = std::move(after)](Vector2_F32& sz) mutable {
        auto rf = std::move(f)(sz);
        return [rf = std::move(rf), before = std::move(before),
                after = std::move(after)](const Rect2_F32& r) mutable {
            unsized(std::move(before))(r);
            std::move(rf)(r);
            unsized(std::move(after))(r);
        };
    };
};

auto empty(Vector2_F32& sz) {
    sz = {0.f, 0.f};
    return [](const Rect2_F32&) {};
};

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
    return layout([=](const Linear_Vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x = std::max(sz.x, c.x);
            sz.y += c.y + space;
        }
        sz.y = std::max(sz.y - space, 0.f);
        return [=](Linear_Vector<Rect2_F32>& cs, const Rect2_F32& r) {
            auto pos = r.pos;
            for (auto& c : cs) {
                c.pos = pos;
                pos.y += c.size.y + space;
                if (xfill)
                    c.size.x = r.size.x;
            }
        };
    });
};

auto hstack(auto... options) {
    const auto space = *grab<Spacing>({0.f}, options...);
    const auto yfill = *grab<Fill_Crossaxis>({false}, options...);
    return layout([=](const Linear_Vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz.x += c.x + space;
            sz.y = std::max(sz.y, c.y);
        }
        sz.x = std::max(sz.x - space, 0.f);
        return [=](Linear_Vector<Rect2_F32>& cs, const Rect2_F32& r) {
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
    return layout([=](const Linear_Vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz = glm::max(sz, c);
        }
        return [=](Linear_Vector<Rect2_F32>& cs, const Rect2_F32& r) {
            for (auto& c : cs) {
                c = r;
            }
        };
    });
}

auto padding(auto... options) {
    const auto sides_ = grab<Sides>(Sides::all(0.f), options...);
    return layout([sides_](const Linear_Vector<Vector2_F32>& cs, Vector2_F32& sz) {
        sz = {0.f, 0.f};
        for (const auto& c : cs) {
            sz = glm::max(sz, c);
        }
        sz += sides_.sum();
        return [sides_](Linear_Vector<Rect2_F32>& cs, const Rect2_F32& r) {
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

auto minsize(auto&& inner) {
    return [inner = std::move(inner)](auto... options) mutable {
        const auto minsz = grab<Vector2_F32>({0.f, 0.f}, options...);
        return [minsz, inner = std::move(inner)](Vector2_F32& sz) mutable {
            auto rinner = std::move(inner)(sz);
            sz = glm::max(sz, minsz);
            return [rinner = std::move(rinner)](const Rect2_F32& r) mutable { std::move(rinner)(r); };
        };
    };
}

template <typename... Args>
auto text(fmt::string_view fstr, const Args&... args) {
    const auto s = linear_format(ui_linear_alloc<char>(), fstr, args...);
    return [s = std::move(s)](auto... options) {
        auto& state = UI_State::get();
        const auto color = grab<NVGcolor>(state.colors.fg, options...);
        const auto font = grab<Draw_Font>(Draw_Font::Sans, options...);
        const auto size = *grab<Font_Size>({state.opts.font_size}, options...);
        const auto align = grab<Text_Align>(Text_Align::Left_Middle, options...);
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
};

auto mux(auto&&... fs) {
    return [... fs = std::move(fs)](auto&& x) { (fs(std::forward<decltype(x)>(x)), ...); };
}

auto onclick(auto&& f) {
    return [f = std::move(f)](const Interaction& itr) {
        if (itr.click)
            f();
    };
}

auto interact(auto&& cb) {
    return [cb = std::move(cb)](auto&& inner) mutable {
        const auto key = ui_peek_key("interact");
        return [key, cb = std::move(cb), inner = std::move(inner)](Vector2_F32& sz) mutable {
            auto rinner = std::move(inner)(sz);
            return [key, cb = std::move(cb), rinner = std::move(rinner)](const Rect2_F32& r) mutable {
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

                if (state.input.hover(r)) {
                    state.take_hot(key);
                }

                std::move(cb)(itr);

                rinner(r);
            };
        };
    };
}

auto button(auto&& inner, auto&& cb) {
    return [inner = std::move(inner), cb = std::move(cb)](auto... options) mutable {
        const auto key = ui_push_key(consthash("button"));
        struct S {
            Interaction itr;
        }& s = ui_get_state<S>(key);

        const auto tint_ = grab<Tint>({nvgRGB(0, 0, 0), 0.f}, options...);
        auto r = interact([&s, cb = std::move(cb)](Interaction itr) mutable {
            s.itr = itr;
            std::move(cb)(itr);
        })(
            before(
                padding(Sides::xy(UI_State::get().opts.text_pad))(std::move(inner)),
                drawn({0.f, 0.f}, [&s, tint_](const Rect2_F32& r) {
                    auto& state = UI_State::get();

                    auto from = blend_color(
                        state.colors.button_from, state.colors.button_to,
                        s.itr.press ? 1.f : (s.itr.hover ? 0.5f : 0.f));
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
            const auto textsz = evalsize(text("{}", e)());
            minwidth = std::max(minwidth, textsz.x);
        }

        const auto key = ui_push_key(consthash("dropdown"));

        ui_push_key(consthash("dropdown_menu"));
        auto& state = UI_State::get();
        if (state.has_focus(key)) {
            auto vs = vstack(Fill_Crossaxis{true});
            for (size_t i = 0; i < enums.size(); ++i) {
                const auto itemkey = ui_push_key(consthash("item"), i);
                auto& s = ui_get_state<Interaction>(itemkey);

                static constexpr auto menubutton = [](Interaction& s, size_t i, uint32& index,
                                                      std::string_view e) {
                    return interact([&s, i, &index](Interaction itr) {
                        s = itr;
                        if (itr.click) {
                            index = i;
                        }
                    })(
                        before(
                            padding(Sides::xy(UI_State::get().opts.text_pad))(text("{}", e)()),
                            drawn({0.f, 0.f}, [&s](const Rect2_F32& r) {
                                auto& state = UI_State::get();
                                if (s.hover)
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
            hside(minsize(text("{}", enums[index])())(Vector2_F32{minwidth, 0.f}), text("⯆")())(Spacing{5.f}),
            [=](Interaction itr) {
                if (itr.click)
                    UI_State::get().take_focus(key);
            })();

        ui_pop_key(); // dropdown
        return r;
    };
}

using Placeholder = Value<Linear_String>;

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
        }& s = ui_get_state<S>(key);

        auto zs = chain(
            zstack(),
            //
            drawn(
                {},
                [&s, &state](const Rect2_F32& r) {
                    state.draw->fill_rrect(
                        r, state.opts.corner_radius,
                        s.itr.focus ? state.colors.input_focus_bg : state.colors.input_bg);

                    state.draw->stroke_rrect(
                        r.half_round(), state.opts.corner_radius,
                        s.itr.focus ? state.colors.focus_border : state.colors.border,
                        state.opts.border_width);
                }),
            //
            minsize(interact([&s, &state, &txt](Interaction itr) {
                s.itr = itr;
                if (itr.focus) {
                    if (s.cursor > 0 && state.input.keys_just_pressed[GLFW_KEY_LEFT]) {
                        --s.cursor;
                    }
                    if (s.cursor < txt.size() && state.input.keys_just_pressed[GLFW_KEY_RIGHT]) {
                        ++s.cursor;
                    }

                    if (state.input.text) {
                        txt.insert(txt.begin() + s.cursor++, *state.input.text);
                    }

                    if (s.cursor > 0 && state.input.keys_just_pressed[GLFW_KEY_BACKSPACE]) {
                        txt.erase(txt.begin() + --s.cursor);
                    }

                    if (state.input.keys_just_pressed[GLFW_KEY_ENTER]) {
                        state.take_focus(UI_Key::null());
                    }
                }
            })(padding(Sides::xy(state.opts.text_pad))(text("{}", txt)(Draw_Font::Mono))))(
                Vector2_F32{width_, 0.f}),
            //
            iff(s.itr.focus, drawn({}, [=, &s, &state, &txt](const Rect2_F32& r) {
                    float32 cwidth = 0.f;
                    state.draw->measure_text("A", Draw_Font::Mono, font_size_, &cwidth);

                    state.draw->fill_rrect(
                        {r.pos + Vector2_F32{cwidth * s.cursor + state.opts.text_pad.x, 2.f},
                         {cwidth, r.size.y - 4.f}},
                        0.f, state.colors.fg);

                    if (s.cursor < txt.size()) {
                        state.draw->text(
                            r.pos + state.opts.text_pad + Vector2_F32{cwidth * s.cursor, font_size_ / 2.f},
                            Text_Align::Left_Middle, txt.substr(s.cursor, 1), state.colors.input_focus_bg,
                            Draw_Font::Mono, font_size_);
                    }
                })));

        ui_pop_key(); // textbox

        return zs;
    };
}

} // namespace
} // namespace ui