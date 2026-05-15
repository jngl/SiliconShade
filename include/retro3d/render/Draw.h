#ifndef INC_3D_TEST_DRAW_H
#define INC_3D_TEST_DRAW_H

#include <retro3d/core/Memory.h>

void draw_clear_buffer(View<uint32_t> buffer, uint32_t color);
void draw_clear_depth_buffer(View<int32_t> buffer, int32_t depth);

#endif //INC_3D_TEST_DRAW_H
