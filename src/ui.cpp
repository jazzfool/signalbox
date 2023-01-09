#include "ui.h"

#include "state.h"
#include "draw.h"

#include <string_view>

ui_vstack::ui_vstack(ui_state& state) {
    const auto key = state.key.next(std::string_view{"vstack"});
    m_state = &state.memory->state<self_state>(key);
}

vector2f32 ui_vstack::last_size() {
    const auto sized = m_state->sized;
    m_state->sized = {};
    return sized;
}

void ui_vstack::allocate_self(ui_layout& layout) {
    const auto sz = last_size();
    rect = layout.allocate(sz);
}

rect2f32 ui_vstack::allocate(vector2f32 size) {
    m_state->sized.x = std::max(m_state->sized.x, size.x);
    m_state->sized.y += size.y + spacing;

    const auto pos = rect.pos;
    rect.pos.y += size.y + spacing;

    return {pos, size};
}

ui_button::ui_button(ui_state& state) {
    text_color = state.colors->fg;
    tint = nvgRGB(0, 0, 0);
    tint_strength = 0.f;
    font = draw_font::sans;
    size = 12.f;
}

bool ui_button::view(ui_state& state, ui_layout& layout) {
    const auto sz = state.draw->measure_text(text, font, size) + vector2f32{10.f, 7.f};
    const auto rect = layout.allocate(sz).half_round();

    const auto hover = state.input->take_hover(rect);
    const auto clicked = hover && state.input->mouse_just_pressed[0];
    const auto pressed = hover && state.input->mouse_is_pressed[0];

    auto from =
        blend_color(state.colors->button_from, state.colors->button_to, pressed ? 1.f : (hover ? 0.5f : 0.f));
    auto to = state.colors->button_to;

    from = blend_color(from, tint, tint_strength);
    to = blend_color(to, tint, tint_strength);

    state.draw->tb_grad_fill_rrect(rect, 2.f, from, to);

    state.draw->stroke_rrect(rect, 2.f, state.colors->button_border, 1.f);

    state.draw->centered_text((rect.min() + rect.max()) / 2.f, text, text_color, font, size);

    return clicked;
}

void ui_dropdown_box::view(ui_state& state, ui_layout& layout) {
    sb_ASSERT(index);

    auto substate = state.with_next_key(std::string_view{"dropdown"});
    const auto key = substate.key;

    static std::vector<std::string> fallback = {{"No options"}};

    const std::span<std::string> options_span = options.empty() ? fallback : options;

    *index = std::min<uint32>(*index, options_span.size() - 1);

    ui_button btn{state};
    btn.text = options_span[*index];

    if (btn.view(state, layout)) {
        state.take_focus(key);
    }

    if (*state.focus == key) {
        state.draw->push_layer();

        ui_vstack vs{substate};

        const rect2f32 rect{{10.f, 10.f}, vs.last_size()};

        vs.rect = rect;
        vs.spacing = 3.f;

        state.input->begin_overlay_hover(rect);

        for (size_t i = 0; i < options.size(); ++i) {
            ui_button opt_btn{state};
            opt_btn.text = options[i];
            if (opt_btn.view(state, vs)) {
                *index = i;
                state.take_focus(ui_key::null());
            }
        }

        state.input->end_overlay_hover();

        state.input->push_overlay(rect);

        state.draw->pop_layer();
    }
}
