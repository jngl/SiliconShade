#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/render/Rasterizer.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

struct TestVarying {
    float r, g, b;
};

// Helper for buffer verification
bool is_color_near(uint32_t color, uint8_t r, uint8_t g, uint8_t b, uint8_t tolerance = 5) {
    uint8_t br = (color >> 16) & 0xFF;
    uint8_t bg = (color >> 8) & 0xFF;
    uint8_t bb = color & 0xFF;
    return std::abs(br - r) <= tolerance && std::abs(bg - g) <= tolerance && std::abs(bb - b) <= tolerance;
}

TEST_CASE("Rasterizer Operations") {
    const int w = 100, h = 100;
    std::vector<uint32_t> buffer(w * h, 0);
    AABB screen = {{0, 0}, {w - 1, h - 1}};

    SUBCASE("Interpolation Accuracy") {
        auto vs = [](int i) {
            Rasterizer::VSOutput<TestVarying> out;
            out.inv_w = 1 << 16;
            if (i == 0) { // Top-left
                out.position = {0, 0, 0, 0};
                out.varying = {255.0f, 0, 0};
            } else if (i == 1) { // Top-right
                out.position = {99 << Rasterizer::SUBPIXEL_SHIFT, 0, 0, 0};
                out.varying = {0, 255.0f, 0};
            } else { // Bottom-left
                out.position = {0, 99 << Rasterizer::SUBPIXEL_SHIFT, 0, 0};
                out.varying = {0, 0, 255.0f};
            }
            return out;
        };

        auto fs = [&](int32_t x, int32_t y, int mask, __m256 b0, __m256 b1, __m256 b2,
                      const auto& v0, const auto& v1, const auto& v2) {
            alignas(32) float ab0[8], ab1[8], ab2[8];
            _mm256_store_ps(ab0, b0); _mm256_store_ps(ab1, b1); _mm256_store_ps(ab2, b2);
            for (int i = 0; i < 8; ++i) {
                if (mask & (1 << i)) {
                    int r = (int)(v0.varying.r * ab0[i] + v1.varying.r * ab1[i] + v2.varying.r * ab2[i]);
                    int g = (int)(v0.varying.g * ab0[i] + v1.varying.g * ab1[i] + v2.varying.g * ab2[i]);
                    int b = (int)(v0.varying.b * ab0[i] + v1.varying.b * ab1[i] + v2.varying.b * ab2[i]);
                    buffer[y * w + (x + i)] = (r << 16) | (g << 8) | b;
                }
            }
        };

        Rasterizer::draw_triangle<TestVarying>(vs, fs, screen);
        CHECK(is_color_near(buffer[0], 255, 0, 0));
        CHECK(buffer[30 * w + 30] != 0); 
    }

    SUBCASE("Depth Buffer") {
        std::vector<int32_t> depth_buffer(w * h, -1);
        std::fill(buffer.begin(), buffer.end(), 0);

        auto draw_tri = [&](int32_t z_val, uint32_t color) {
            auto vs = [=](int i) {
                Rasterizer::VSOutput<TestVarying> out;
                out.inv_w = z_val;
                if (i == 0) out.position = {0, 0, 0, 0};
                else if (i == 1) out.position = {99 << Rasterizer::SUBPIXEL_SHIFT, 0, 0, 0};
                else out.position = {0, 99 << Rasterizer::SUBPIXEL_SHIFT, 0, 0};
                out.varying = {(float)((color >> 16) & 0xFF), (float)((color >> 8) & 0xFF), (float)(color & 0xFF)};
                return out;
            };

            auto fs = [&](int32_t x, int32_t y, int mask, __m256 b0, __m256 b1, __m256 b2,
                          const auto& v0, const auto& v1, const auto& v2) {
                alignas(32) float ab0[8], ab1[8], ab2[8];
                _mm256_store_ps(ab0, b0); _mm256_store_ps(ab1, b1); _mm256_store_ps(ab2, b2);
                for (int i = 0; i < 8; ++i) {
                    if (mask & (1 << i)) {
                        int32_t pixel_idx = y * w + (x + i);
                        int32_t depth = (int32_t)(v0.inv_w * ab0[i] + v1.inv_w * ab1[i] + v2.inv_w * ab2[i]);
                        if (depth > depth_buffer[pixel_idx]) {
                            depth_buffer[pixel_idx] = depth;
                            buffer[pixel_idx] = (uint32_t)v0.varying.r << 16 | (uint32_t)v0.varying.g << 8 | (uint32_t)v0.varying.b;
                        }
                    }
                }
            };
            Rasterizer::draw_triangle<TestVarying>(vs, fs, screen);
        };

        draw_tri(100, 0x0000FF);
        draw_tri(200, 0xFF0000);
        CHECK(is_color_near(buffer[10 * w + 10], 255, 0, 0));

        draw_tri(300, 0x00FF00);
        CHECK(is_color_near(buffer[10 * w + 10], 0, 255, 0));

        draw_tri(50, 0xFFFFFF);
        CHECK(is_color_near(buffer[10 * w + 10], 0, 255, 0));
    }

    SUBCASE("AABB Clipping") {
        AABB clip_screen = {{25, 25}, {75, 75}};
        int pixels_drawn = 0;
        
        auto vs = [](int i) {
            Rasterizer::VSOutput<TestVarying> out;
            out.inv_w = 1;
            if (i == 0) out.position = {0, 0, 0, 0};
            else if (i == 1) out.position = {200 << Rasterizer::SUBPIXEL_SHIFT, 0, 0, 0};
            else out.position = {0, 200 << Rasterizer::SUBPIXEL_SHIFT, 0, 0};
            return out;
        };

        auto fs = [&](int32_t x, int32_t y, int mask, __m256, __m256, __m256, const auto&, const auto&, const auto&) {
            for (int i = 0; i < 8; ++i) {
                if (mask & (1 << i)) {
                    int px = x + i;
                    if (px >= 25 && px <= 75 && y >= 25 && y <= 75) {
                        pixels_drawn++;
                    }
                }
            }
        };

        Rasterizer::draw_triangle<TestVarying>(vs, fs, clip_screen);
        CHECK(pixels_drawn == 51 * 51);
    }

    SUBCASE("Degenerate Triangles") {
        auto vs = [](int i) {
            Rasterizer::VSOutput<TestVarying> out;
            out.inv_w = 1;
            out.position = {10 << Rasterizer::SUBPIXEL_SHIFT, 10 << Rasterizer::SUBPIXEL_SHIFT, 0, 0};
            return out;
        };

        bool drawn = false;
        auto fs = [&](auto...) { drawn = true; };

        Rasterizer::draw_triangle<TestVarying>(vs, fs, screen);
        CHECK_FALSE(drawn);
    }
}

TEST_CASE("Performance Benchmark" * doctest::skip(true)) {
    const int w = 1920, h = 1080;
    std::vector<uint32_t> buffer(w * h, 0);
    AABB screen = {{0, 0}, {w - 1, h - 1}};

    auto vs = [](int i) {
        Rasterizer::VSOutput<TestVarying> out;
        out.inv_w = 1;
        if (i == 0) out.position = {0, 0, 0, 0};
        else if (i == 1) out.position = {1919 << Rasterizer::SUBPIXEL_SHIFT, 0, 0, 0};
        else out.position = {0, 1079 << Rasterizer::SUBPIXEL_SHIFT, 0, 0};
        return out;
    };

    auto fs = [&](int32_t x, int32_t y, int mask, __m256, __m256, __m256, const auto&, const auto&, const auto&) {
        for(int i=0; i<8; ++i) if(mask & (1<<i)) buffer[y*w + x + i] = 0xFFFFFFFF;
    };

    auto start = std::chrono::high_resolution_clock::now();
    const int frames = 1000;
    for(int f=0; f<frames; ++f) {
        Rasterizer::draw_triangle<TestVarying>(vs, fs, screen);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    MESSAGE("Benchmark: ", frames, " triangles in ", ms, "ms (", (double)ms/frames, "ms/tri)");
}
