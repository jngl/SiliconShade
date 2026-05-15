#ifndef INC_RETRO3D_INPUT_H
#define INC_RETRO3D_INPUT_H

#include <cstdint>
#include <vector>
#include <unordered_map>

class Input {
public:
    enum Key {
        K_W, K_A, K_S, K_D,
        K_UP, K_DOWN, K_LEFT, K_RIGHT,
        K_ESCAPE, K_SPACE,
        K_COUNT
    };

    Input();

    void set_key_state(Key key, bool pressed);
    bool is_key_down(Key key) const;
    bool is_key_pressed(Key key) const; // Pressed this frame

    void set_mouse_pos(int32_t x, int32_t y);
    void set_mouse_relative(int32_t dx, int32_t dy);
    
    int32_t mouse_x() const { return m_mouse_x; }
    int32_t mouse_y() const { return m_mouse_y; }
    int32_t mouse_dx() const { return m_mouse_dx; }
    int32_t mouse_dy() const { return m_mouse_dy; }

    void clear_frame_data();

private:
    bool m_keys[K_COUNT];
    bool m_keys_last[K_COUNT];
    int32_t m_mouse_x = 0;
    int32_t m_mouse_y = 0;
    int32_t m_mouse_dx = 0;
    int32_t m_mouse_dy = 0;
};

#endif //INC_RETRO3D_INPUT_H
