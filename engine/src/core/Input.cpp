#include <retro3d/core/Input.h>
#include <cstring>

Input::Input() {
    std::memset(m_keys, 0, sizeof(m_keys));
    std::memset(m_keys_last, 0, sizeof(m_keys_last));
}

void Input::set_key_state(Key key, bool pressed) {
    if (key < K_COUNT) {
        m_keys[key] = pressed;
    }
}

bool Input::is_key_down(Key key) const {
    if (key < K_COUNT) {
        return m_keys[key];
    }
    return false;
}

bool Input::is_key_pressed(Key key) const {
    if (key < K_COUNT) {
        return m_keys[key] && !m_keys_last[key];
    }
    return false;
}

void Input::set_mouse_pos(int32_t x, int32_t y) {
    m_mouse_x = x;
    m_mouse_y = y;
}

void Input::set_mouse_relative(int32_t dx, int32_t dy) {
    m_mouse_dx = dx;
    m_mouse_dy = dy;
}

void Input::clear_frame_data() {
    std::memcpy(m_keys_last, m_keys, sizeof(m_keys));
    m_mouse_dx = 0;
    m_mouse_dy = 0;
}
