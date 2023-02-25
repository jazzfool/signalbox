#include "ui.h"

UI_Key UI_Key::root() {
    auto key = UI_Key{0}.next(0);
    return key;
}

UI_Key UI_Key::null() {
    return {};
}

bool UI_Key::is_null() const {
    return m_null;
}

uint64 UI_Key::value() const {
    sb_ASSERT(!is_null());
    return m_hash;
}

bool UI_Key::operator==(const UI_Key& rhs) const {
    return !(is_null() != rhs.is_null() || value() != rhs.value());
}

bool UI_Key::operator!=(const UI_Key& rhs) const {
    return !(*this == rhs);
}

UI_Key::UI_Key(uint64 v) : m_null{false}, m_hash{v} {
}

UI_Key::UI_Key() : m_null{true}, m_hash{UINT64_MAX} {
}

void UI_Memory::begin_frame() {
    m_key_stack.clear();
    m_key_stack.emplace_back(UI_Key::root(), 0);
    m_next_ikey = 0;

    auto alloc_resize = false;
    if (m_linear) {
        const auto allocated = m_linear->report_allocated();
        alloc_resize = allocated > m_max_linear;
        m_max_linear = std::max(m_max_linear, allocated);
    } else {
        alloc_resize = true;
        m_max_linear = 8192;
    }

    if (alloc_resize) {
        spdlog::info("+uimem {}B", m_max_linear);
        m_linear.emplace(m_max_linear);
    }

    m_linear->deallocate();
}

void UI_Memory::push_existing_key(UI_Key key) {
    m_key_stack.emplace_back(key, m_next_ikey);
    m_next_ikey = 0;
}

UI_State& UI_State::get() {
    static UI_State s;
    return s;
}

void UI_State::begin_frame(Draw_List& out) {
    input.begin_frame();
    memory.begin_frame();

    draw = &out;

    hot_taken = false;
    focus_taken = false;
}

void UI_State::end_frame() {
    for (auto& [overlay, rect] : overlays) {
        (*overlay)(rect);
    }
    overlays.clear();

    draw = nullptr;

    if (!hot_taken)
        hot = UI_Key::null();

    if (!focus_taken && input.mouse_just_pressed[0])
        focus = UI_Key::null();

    prev_hot = hot;
    prev_focus = focus;

    input.end_frame();
}

void UI_State::take_hot(UI_Key key) {
    hot = key;
    hot_taken = true;
}

void UI_State::take_focus(UI_Key key) {
    focus = key;
    focus_taken = true;
}

bool UI_State::is_hot(UI_Key key) const {
    return !key.is_null() && key == prev_hot;
}

bool UI_State::has_focus(UI_Key key) const {
    return !key.is_null() && key == prev_focus;
}

void ui_push_existing_key(UI_Key key) {
    return UI_State::get().memory.push_existing_key(key);
}

void ui_pop_key() {
    return UI_State::get().memory.pop_key();
}

Linear_String ui_linear_str(const char* s) {
    return Linear_String{s, ui_linear_alloc<uint8>()};
}
