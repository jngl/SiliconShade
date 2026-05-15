#include <retro3d/render/Draw.h>
#include <cstring>
#include <algorithm>

void draw_clear_buffer(const View<uint32_t> buffer, const uint32_t color) {
    if (color == 0) {
        std::memset(buffer.data, 0, buffer.size * sizeof(uint32_t));
    } else {
        std::fill_n(buffer.data, buffer.size, color);
    }
}

void draw_clear_depth_buffer(const View<int32_t> buffer, const int32_t depth) {
    if (depth == 0) {
        std::memset(buffer.data, 0, buffer.size * sizeof(int32_t));
    } else {
        std::fill_n(buffer.data, buffer.size, depth);
    }
}
