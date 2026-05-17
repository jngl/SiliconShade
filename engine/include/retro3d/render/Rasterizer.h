#ifndef INC_3D_TEST_RASTERIZER_H
#define INC_3D_TEST_RASTERIZER_H

#include <retro3d/core/Math.h>
#include <retro3d/core/ThreadPool.h>
#include <algorithm>
#include <cstdint>
#include <immintrin.h>

namespace Rasterizer {

    constexpr int32_t SUBPIXEL_SHIFT = 4;
    constexpr int32_t SUBPIXEL_MULT = 1 << SUBPIXEL_SHIFT;
    constexpr int32_t TILE_SIZE = 8; 

    template<typename TVarying>
    struct VSOutput {
        Vec4 position; 
        int32_t inv_w; 
        TVarying varying;
    };

    inline int32_t edge_function(const Vec2& a, const Vec2& b, const Vec2& p) {
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    }

    // --- Utilitaires SIMD ---
    inline __m256 interpolate8(__m256 v0, __m256 v1, __m256 v2, __m256 b0, __m256 b1, __m256 b2) {
        return _mm256_add_ps(_mm256_mul_ps(v0, b0), 
               _mm256_add_ps(_mm256_mul_ps(v1, b1), _mm256_mul_ps(v2, b2)));
    }

    template<typename TVarying, typename VS, typename FS>
    void draw_triangle(
        const VS& vertex_shader,
        const FS& fragment_shader,
        const AABB& screen_aabb,
        ThreadPool* pool = nullptr
    ) {
        VSOutput<TVarying> v0 = vertex_shader(0);
        VSOutput<TVarying> v1 = vertex_shader(1);
        VSOutput<TVarying> v2 = vertex_shader(2);

        Vec2 p0 = { v0.position.x, v0.position.y };
        Vec2 p1 = { v1.position.x, v1.position.y };
        Vec2 p2 = { v2.position.x, v2.position.y };

        int32_t min_x = std::min({p0.x, p1.x, p2.x}) >> SUBPIXEL_SHIFT;
        int32_t min_y = std::min({p0.y, p1.y, p2.y}) >> SUBPIXEL_SHIFT;
        int32_t max_x = (std::max({p0.x, p1.x, p2.x}) + SUBPIXEL_MULT - 1) >> SUBPIXEL_SHIFT;
        int32_t max_y = (std::max({p0.y, p1.y, p2.y}) + SUBPIXEL_MULT - 1) >> SUBPIXEL_SHIFT;

        min_x = std::max(min_x, (int32_t)screen_aabb.min.x);
        min_y = std::max(min_y, (int32_t)screen_aabb.min.y);
        max_x = std::min(max_x, (int32_t)screen_aabb.max.x);
        max_y = std::min(max_y, (int32_t)screen_aabb.max.y);

        if (min_x > max_x || min_y > max_y) return;

        int32_t area = edge_function(p0, p1, p2);
        if (area == 0) return; // Triangle dégénéré
        
        // Si l'aire est négative, on inverse les rôles pour toujours avoir une aire positive
        // Cela permet de supporter les deux sens d'enroulement si le culling 3D est désactivé
        if (area < 0) {
            std::swap(p1, p2);
            std::swap(v1, v2);
            area = -area;
        }

        float inv_area_f = 1.0f / (float)area;

        int32_t a01 = p0.y - p1.y, b01 = p1.x - p0.x;
        int32_t a12 = p1.y - p2.y, b12 = p2.x - p1.x;
        int32_t a20 = p2.y - p0.y, b20 = p0.x - p2.x;

        int32_t step_x0 = a12 << SUBPIXEL_SHIFT;
        int32_t step_y0 = b12 << SUBPIXEL_SHIFT;
        int32_t step_x1 = a20 << SUBPIXEL_SHIFT;
        int32_t step_y1 = b20 << SUBPIXEL_SHIFT;
        int32_t step_x2 = a01 << SUBPIXEL_SHIFT;
        int32_t step_y2 = b01 << SUBPIXEL_SHIFT;

        auto worker = [=](int32_t start_ty, int32_t end_ty) {
            __m256i v_step_x0 = _mm256_set1_epi32(step_x0);
            __m256i v_step_x1 = _mm256_set1_epi32(step_x1);
            __m256i v_step_x2 = _mm256_set1_epi32(step_x2);
            __m256i v_step_y0 = _mm256_set1_epi32(step_y0);
            __m256i v_step_y1 = _mm256_set1_epi32(step_y1);
            __m256i v_step_y2 = _mm256_set1_epi32(step_y2);
            
            __m256i v_offsets = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
            __m256i v_x0_inc = _mm256_mullo_epi32(v_step_x0, v_offsets);
            __m256i v_x1_inc = _mm256_mullo_epi32(v_step_x1, v_offsets);
            __m256i v_x2_inc = _mm256_mullo_epi32(v_step_x2, v_offsets);

            __m256 v_inv_area = _mm256_set1_ps(inv_area_f);

            for (int32_t ty = start_ty; ty < end_ty; ty += TILE_SIZE) {
                for (int32_t tx = min_x & ~(TILE_SIZE - 1); tx <= max_x; tx += TILE_SIZE) {
                    
                    Vec2 tile_p = { (tx << SUBPIXEL_SHIFT) + (SUBPIXEL_MULT >> 1),
                                    (ty << SUBPIXEL_SHIFT) + (SUBPIXEL_MULT >> 1) };
                    
                    int32_t w0_base = edge_function(p1, p2, tile_p);
                    int32_t w1_base = edge_function(p2, p0, tile_p);
                    int32_t w2_base = edge_function(p0, p1, tile_p);

                    // Test de rejet de tuile (AABB optimisé)
                    auto is_outside = [&](int32_t w, int32_t a, int32_t b) {
                        return (w + std::max(0, (a * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT)
                                  + std::max(0, (b * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT)) < 0;
                    };

                    if (is_outside(w0_base, a12, b12) || is_outside(w1_base, a20, b20) || is_outside(w2_base, a01, b01)) {
                        continue;
                    }

                    // On initialise les arêtes pour la première ligne de la tuile
                    __m256i v_w0_row = _mm256_add_epi32(_mm256_set1_epi32(w0_base), v_x0_inc);
                    __m256i v_w1_row = _mm256_add_epi32(_mm256_set1_epi32(w1_base), v_x1_inc);
                    __m256i v_w2_row = _mm256_add_epi32(_mm256_set1_epi32(w2_base), v_x2_inc);

                    for (int32_t y = ty; y < ty + TILE_SIZE; ++y) {
                        if (y < min_y || y > max_y) {
                            v_w0_row = _mm256_add_epi32(v_w0_row, v_step_y0);
                            v_w1_row = _mm256_add_epi32(v_w1_row, v_step_y1);
                            v_w2_row = _mm256_add_epi32(v_w2_row, v_step_y2);
                            continue;
                        }

                        // Déterminer quels pixels sont à l'intérieur
                        __m256i v_mask = _mm256_or_si256(_mm256_or_si256(v_w0_row, v_w1_row), v_w2_row);
                        __m256i v_inside = _mm256_cmpgt_epi32(v_mask, _mm256_set1_epi32(-1));
                        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(v_inside));

                        // Clamp to screen/AABB boundaries horizontally
                        if (tx + 7 > max_x) {
                            int boundary_mask = (1 << (max_x - tx + 1)) - 1;
                            mask &= boundary_mask;
                        }

                        if (mask != 0) {
                            // Interpolation SIMD (en flottants)
                            __m256 v_fw0 = _mm256_cvtepi32_ps(v_w0_row);
                            __m256 v_fw1 = _mm256_cvtepi32_ps(v_w1_row);
                            __m256 v_fw2 = _mm256_cvtepi32_ps(v_w2_row);

                            __m256 b0 = _mm256_mul_ps(v_fw0, v_inv_area);
                            __m256 b1 = _mm256_mul_ps(v_fw1, v_inv_area);
                            __m256 b2 = _mm256_mul_ps(v_fw2, v_inv_area);

                            // Vectorized fragment shader call
                            fragment_shader(tx, y, mask, b0, b1, b2, v0, v1, v2);
                        }

                        // Passage à la ligne suivante (addition au lieu de multiplication)
                        v_w0_row = _mm256_add_epi32(v_w0_row, v_step_y0);
                        v_w1_row = _mm256_add_epi32(v_w1_row, v_step_y1);
                        v_w2_row = _mm256_add_epi32(v_w2_row, v_step_y2);
                    }
                }
            }
        };

        if (pool) {
            int32_t tile_min_y = min_y & ~(TILE_SIZE - 1);
            int32_t total_tiles_y = (max_y - tile_min_y) / TILE_SIZE + 1;
            int32_t num_threads = (int32_t)pool->get_num_threads();
            int32_t tiles_per_thread = (total_tiles_y + num_threads - 1) / num_threads;

            for (int32_t i = 0; i < num_threads; ++i) {
                int32_t start_tile = i * tiles_per_thread;
                int32_t end_tile = std::min((i + 1) * tiles_per_thread, total_tiles_y);
                if (start_tile < end_tile) {
                    pool->enqueue([=] {
                        worker(tile_min_y + start_tile * TILE_SIZE, tile_min_y + end_tile * TILE_SIZE);
                    });
                }
            }
            pool->wait_all();
        } else {
            worker(min_y & ~(TILE_SIZE - 1), max_y + 1);
        }
    }

} // namespace Rasterizer

#endif //INC_3D_TEST_RASTERIZER_H
