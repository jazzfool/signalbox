#include "ui.h"

#include "draw.h"
#include "state.h"

#include <string_view>

ui_layout::ui_layout(std::string_view namekey) : m_key{ui_state::get().push_key(namekey)} {
    m_state = &ui_get_state<self_state>(*m_key);
    m_min_size = m_state->min_size;
}

vector2f32 ui_layout::min_size() {
    return m_min_size;
}

void ui_layout::allocate_self(ui_layout& layout) {
    rect = layout.allocate(min_size());
}

rect2f32 ui_layout::allocate(vector2f32 size) {
    const auto rect = allocate_rect(size);
    m_last = rect;
    return rect;
}

rect2f32 ui_layout::last_allocated() const {
    return m_last;
}

void ui_layout::begin() {
    m_state->min_size = {0.f, 0.f};
}

ui_key ui_layout::key() const {
    return m_key.get();
}

vector2f32& ui_layout::get_min_size() {
    return m_state->min_size;
}

ui_exact::ui_exact() : ui_layout{"exact"}, m_once{false} {
}

rect2f32 ui_exact::allocate_rect(vector2f32 size) {
    sb_ASSERT(!m_once);
    m_once = true;

    const auto r = rect;
    rect = {};

    auto& min_sz = get_min_size();
    min_sz = glm::max(min_sz, size);

    return r;
}

ui_sized::ui_sized() : ui_layout{"sized"} {
}

rect2f32 ui_sized::allocate_rect(vector2f32 size) {
    auto& min_sz = get_min_size();
    min_sz = glm::max(min_sz, size);

    return {rect.pos, size};
}

ui_fixed::ui_fixed(vector2f32 size) : ui_layout{"fixed"}, m_size{size} {
    auto& min_sz = get_min_size();
    if (size.x > 0.f) {
        min_sz.x = size.x;
    }
    if (size.y > 0.f) {
        min_sz.y = size.y;
    }
}

rect2f32 ui_fixed::allocate_rect(vector2f32 size) {
    auto out_sz = m_size;
    if (m_size.x == 0.f) {
        out_sz.x = size.x;
    }
    if (m_size.y == 0.f) {
        out_sz.y = size.y;
    }

    auto& min_sz = get_min_size();
    min_sz = glm::max(min_sz, out_sz);

    return {rect.pos, out_sz};
}

ui_padding::ui_padding() : ui_layout{"padding"}, m_once{false} {
}

rect2f32 ui_padding::allocate_rect(vector2f32 size) {
    sb_ASSERT(!m_once);
    m_once = true;

    const auto pos = rect.pos;
    rect = {};

    const auto padded = size + vector2f32{left + right, top + bottom};
    get_min_size() = padded;

    return {pos + vector2f32{left, top}, size};
}

ui_row2::ui_row2() : ui_layout{"row2"}, m_i{0} {
}

rect2f32 ui_row2::allocate_rect(vector2f32 size) {
    sb_ASSERT(m_i < 2);

    auto& min_sz = get_min_size();
    min_sz.x += size.x;
    min_sz.y = std::max(min_sz.y, size.y);

    if (m_i == 0) {
        ++m_i;
        min_sz.x += spacing;
        rect.size.x -= size.x + spacing;
        return {{rect.max().x + spacing, rect.pos.y}, size};
    } else {
        return {rect.pos, size};
    }
}

ui_vstack::ui_vstack() : ui_layout{"vstack"} {
}

rect2f32 ui_vstack::allocate_rect(vector2f32 size) {
    auto& min_sz = get_min_size();

    min_sz.x = std::max(min_sz.x, size.x);
    min_sz.y += size.y + spacing;

    const auto pos = rect.pos;
    rect.pos.y += size.y + spacing;

    return {pos, size};
}

ui_text_opts::ui_text_opts() {
    auto& state = ui_state::get();

    color = state.colors->fg;
    font = draw_font::sans;
    font_size = state.opts->font_size;
    align = text_align::left_middle;
}

ui_text_opts ui_text_opts::with_text(std::string_view text) {
    ui_text_opts opts;
    opts.text = text;
    return opts;
}

vector2f32 ui_text_opts::measure() const {
    return ui_state::get().draw->measure_text(text, font, font_size);
}

void ui_text(const ui_text_opts& opts) {
    auto& state = ui_state::get();

    const auto sz = opts.measure();
    const auto rect = state.active_layout().allocate(sz);

    const auto pos = opts.align == text_align::center_middle
                         ? (rect.min() + rect.max()) / 2.f
                         : vector2f32{rect.min().x, (rect.min().y + rect.max().y) / 2.f};

    state.draw->text(pos, opts.align, opts.text, opts.color, opts.font, opts.font_size);
}

ui_button_opts::ui_button_opts() {
    tint = nvgRGB(0, 0, 0);
    tint_strength = 0.f;
}

bool ui_button(const ui_button_opts& opts, ui_func&& inner) {
    auto& state = ui_state::get();

    auto clicked = false;

    ui_sized sized;
    ui_with_layout(sized, [&] {
        ui_padding padding;
        padding.left = state.opts->text_pad_x;
        padding.right = state.opts->text_pad_x;
        padding.top = state.opts->text_pad_y;
        padding.bottom = state.opts->text_pad_y;
        ui_with_layout(padding, [&] {
            const auto rect = sized.rect.half_round();

            const auto hover = state.input->take_hover(rect);
            const auto pressed = hover && state.input->mouse_is_pressed[0];
            clicked = hover && state.input->mouse_just_pressed[0];

            auto from = blend_color(
                state.colors->button_from, state.colors->button_to, pressed ? 1.f : (hover ? 0.5f : 0.f));
            auto to = state.colors->button_to;

            from = blend_color(from, opts.tint, opts.tint_strength);
            to = blend_color(to, opts.tint, opts.tint_strength);

            state.draw->tb_grad_fill_rrect(rect, state.opts->corner_radius, from, to);

            state.draw->stroke_rrect(
                rect, state.opts->corner_radius, state.colors->button_border, state.opts->border_width);

            inner();
        });
    });

    return clicked;
}

bool ui_text_button(const ui_button_opts& opts, std::string_view text) {
    return ui_button(
        opts, //
        ui_lambda([&] {
            ui_text_opts topts = ui_text_opts::with_text(text);
            topts.color = blend_color(topts.color, opts.tint, opts.tint_strength);
            ui_text(topts);
        }));
}

void ui_dropdown_box(const ui_dropdown_box_opts& opts) {
    sb_ASSERT(opts.index);

    auto& state = ui_state::get();

    const auto key = state.push_key(std::string_view{"dropdown"});

    static std::vector<std::string> fallback = {{"No options"}};

    const std::span<const std::string> options_span = opts.options.empty() ? fallback : opts.options;

    *opts.index = std::min<uint32>(*opts.index, options_span.size() - 1);

    ui_exact popup_layout;

    const auto clicked = ui_button(
        {}, //
        ui_lambda([&] {
            ui_fixed fixed{{popup_layout.min_size().x + 5.f, 0.f}};
            ui_with_layout(fixed, [&] {
                ui_row2 row;
                ui_with_layout(row, [&] {
                    ui_text(ui_text_opts::with_text("⯆"));
                    ui_text(ui_text_opts::with_text(opts.options[*opts.index]));
                });
            });
        }));

    if (clicked) {
        state.take_focus(*key);
    }

    const auto btn_rect = state.active_layout().last_allocated();

    if (*state.focus == *key) {
        ui_with_no_alloc_layout(popup_layout, [&]() {
            ui_menu_opts menu;
            menu.spacing = 0.f;
            menu.options = opts.options;
            menu.current = *opts.index;

            popup_layout.rect = {
                btn_rect.pos - vector2f32{0.f, (btn_rect.size.y + menu.spacing) * (float32)(*opts.index)},
                popup_layout.min_size()};

            const auto selected = ui_menu(menu);
            if (selected != UINT32_MAX) {
                state.take_focus(ui_key::null());
                *opts.index = selected;
            }
        });
    }
}

ui_menu_opts::ui_menu_opts() {
    auto& state = ui_state::get();

    font_size = state.opts->font_size;
    font = draw_font::sans;
    spacing = 0.f;
    current = UINT32_MAX;
}

uint32 ui_menu(const ui_menu_opts& opts) {
    auto& state = ui_state::get();

    const auto key = state.push_key(std::string_view{"menu"});

    ui_vstack vs;
    vs.spacing = opts.spacing;

    uint32 selected = UINT32_MAX;
    ui_with_layout(vs, [&] {
        const auto rect = vs.rect;

        state.draw->push_layer();
        state.input->push_overlay(rect);
        state.input->begin_overlay_hover(rect);

        state.draw->fill_rrect(rect.translate({3.f, 3.f}), state.opts->corner_radius, state.colors->border);
        state.draw->fill_rrect(rect, state.opts->corner_radius, state.colors->bg);

        state.draw->stroke_rrect(
            rect.half_round(), state.opts->corner_radius, state.colors->border, state.opts->border_width);

        for (size_t i = 0; i < opts.options.size(); ++i) {
            auto sz = state.draw->measure_text(opts.options[i], opts.font, opts.font_size) +
                      vector2f32{state.opts->text_pad_x * 2.f, state.opts->text_pad_y * 2.f};
            sz.x = std::max(sz.x, rect.size.x);

            const auto item_rect = vs.allocate(sz);

            auto highlight = i == opts.current;
            auto bg = state.colors->lowlight_bg;
            auto fg = state.colors->lowlight_fg;
            const auto hover = state.input->take_hover(item_rect);

            if (hover) {
                highlight = true;
                bg = state.colors->highlight_bg;
                fg = state.colors->highlight_fg;
            }

            if (highlight) {
                state.draw->fill_rrect(item_rect.inflate(-1.f), state.opts->corner_radius, bg);
            }

            state.draw->text(
                {item_rect.pos.x + state.opts->text_pad_x, (item_rect.pos.y + item_rect.max().y) / 2.f},
                text_align::left_middle, opts.options[i], fg, opts.font, opts.font_size);

            if (hover && state.input->mouse_just_pressed[0]) {
                selected = i;
            }
        }

        state.input->end_overlay_hover();
        state.draw->pop_layer();
    });

    return selected;
}
