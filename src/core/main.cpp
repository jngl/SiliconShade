#include <retro3d/core/App.h>
#include <retro3d/core/Basic.h>
#include <retro3d/render/Draw.h>
#include <retro3d/core/Math.h>
#include <retro3d/core/Memory.h>
#include <retro3d/render/Rasterizer.h>
#include <retro3d/render/Texture.h>
#include <retro3d/render/Model.h>
#include <iostream>
#include <vector>

struct Varying {
    int32_t u_pw, v_pw;
};

struct TransformedVertex {
    Vec3f view_pos;
    Rasterizer::VSOutput<Varying> vs_out;
};

int main() {
    SystemMemory app_memory(GIGABYTES(1));
    Arena app_arena(app_memory);

    App app(app_arena);
    if (!app.init_success()) return 1;

    const char* model_path = "chest.obj";
    const char* tex_path = "Dungeons_Texture_01.png";

    Model model;
    if (!model.load(model_path)) {
        // Try absolute path if relative fails, or just fail
        std::cerr << "Failed to load model" << std::endl;
        return 1;
    }

    Texture texture(tex_path);
    if (!texture.is_valid()) {
        std::cerr << "Failed to load texture" << std::endl;
        return 1;
    }

    Vec3f center = model.get_center();
    float scale = 400.0f / (model.get_bounding_sphere_radius() > 0 ? model.get_bounding_sphere_radius() : 1.0f);

    float rotation = 0;
    AABB screen_aabb = {{0, 0}, {App::WIN_WIDTH - 1, App::WIN_HEIGHT - 1}};

    std::vector<TransformedVertex> vertex_cache;

    while (app.is_running()) {
        app.process_input();
        draw_clear_buffer(app.color_buffer, 0xFF151515);
        draw_clear_depth_buffer(app.depth_buffer, 0);

        Mat4 model_mat = Mat4::rotate_y(rotation);
        rotation += 0.01f;

        for (const auto& mesh : model.get_meshes()) {
            vertex_cache.resize(mesh.vertices.size());

            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                const Vertex& v = mesh.vertices[i];
                Vec3f p = (v.position - center) * scale;
                Vec3f view_p = model_mat.transform(p);
                view_p.z += 600;

                vertex_cache[i].view_pos = view_p;
                
                float f = 800.0f / view_p.z;
                auto& out = vertex_cache[i].vs_out;
                out.position = { (int32_t)(view_p.x * f + (App::WIN_WIDTH / 2)), (int32_t)(-view_p.y * f + (App::WIN_HEIGHT / 2)), 0, 0 };
                out.position.x <<= Rasterizer::SUBPIXEL_SHIFT;
                out.position.y <<= Rasterizer::SUBPIXEL_SHIFT;
                out.inv_w = (int32_t)((1.0f / view_p.z) * (1 << 24));
                out.varying.u_pw = (int32_t)(((int64_t)(v.texcoord.x * 65536.0f) * out.inv_w) >> 16);
                out.varying.v_pw = (int32_t)(((int64_t)(v.texcoord.y * 65536.0f) * out.inv_w) >> 16);
            }

            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                const TransformedVertex& tv0 = vertex_cache[mesh.indices[i]];
                const TransformedVertex& tv1 = vertex_cache[mesh.indices[i+1]];
                const TransformedVertex& tv2 = vertex_cache[mesh.indices[i+2]];

                Vec3f edge1 = tv1.view_pos - tv0.view_pos;
                Vec3f edge2 = tv2.view_pos - tv0.view_pos;
                Vec3f normal = Vec3f::cross(edge1, edge2);
                if (normal.z >= 0) continue; 

                auto vs = [&](int v_idx) {
                    if (v_idx == 0) return tv0.vs_out;
                    if (v_idx == 1) return tv1.vs_out;
                    return tv2.vs_out;
                };

                auto fs = [&](int32_t x, int32_t y, int mask, __m256 b0, __m256 b1, __m256 b2,
                              const Rasterizer::VSOutput<Varying>& v0, const Rasterizer::VSOutput<Varying>& v1, const Rasterizer::VSOutput<Varying>& v2) {
                    
                    __m256 v_inv_w0 = _mm256_set1_ps((float)v0.inv_w);
                    __m256 v_inv_w1 = _mm256_set1_ps((float)v1.inv_w);
                    __m256 v_inv_w2 = _mm256_set1_ps((float)v2.inv_w);

                    // Interpolate inv_w: v0.inv_w * b0 + v1.inv_w * b1 + v2.inv_w * b2
                    __m256 v_inv_w = _mm256_add_ps(_mm256_mul_ps(v_inv_w0, b0), 
                                     _mm256_add_ps(_mm256_mul_ps(v_inv_w1, b1), _mm256_mul_ps(v_inv_w2, b2)));

                    int32_t pixel_idx = y * App::WIN_WIDTH + x;
                    
                    // Load current depth values
                    __m256i v_current_depth = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&app.depth_buffer.data[pixel_idx]));
                    
                    // Convert interpolated inv_w to int32 for comparison
                    __m256i v_new_depth = _mm256_cvtps_epi32(v_inv_w);
                    
                    // Depth test: new_depth > current_depth
                    __m256i v_depth_mask = _mm256_cmpgt_epi32(v_new_depth, v_current_depth);
                    
                    // Combine with coverage mask
                    int final_mask = mask & _mm256_movemask_ps(_mm256_castsi256_ps(v_depth_mask));
                    if (final_mask == 0) return;

                    // Perspective-correct interpolation of UVs
                    // w = 1.0 / inv_w (using 1 << 24 scaling)
                    __m256 v_w = _mm256_div_ps(_mm256_set1_ps(16777216.0f), v_inv_w);
                    
                    __m256 v_u_pw0 = _mm256_set1_ps((float)v0.varying.u_pw);
                    __m256 v_u_pw1 = _mm256_set1_ps((float)v1.varying.u_pw);
                    __m256 v_u_pw2 = _mm256_set1_ps((float)v2.varying.u_pw);
                    __m256 v_u_pw = _mm256_add_ps(_mm256_mul_ps(v_u_pw0, b0), 
                                    _mm256_add_ps(_mm256_mul_ps(v_u_pw1, b1), _mm256_mul_ps(v_u_pw2, b2)));
                    
                    __m256 v_v_pw0 = _mm256_set1_ps((float)v0.varying.v_pw);
                    __m256 v_v_pw1 = _mm256_set1_ps((float)v1.varying.v_pw);
                    __m256 v_v_pw2 = _mm256_set1_ps((float)v2.varying.v_pw);
                    __m256 v_v_pw = _mm256_add_ps(_mm256_mul_ps(v_v_pw0, b0), 
                                    _mm256_add_ps(_mm256_mul_ps(v_v_pw1, b1), _mm256_mul_ps(v_v_pw2, b2)));

                    __m256 v_u = _mm256_mul_ps(_mm256_mul_ps(v_u_pw, v_w), _mm256_set1_ps(1.0f / 65536.0f));
                    __m256 v_v = _mm256_mul_ps(_mm256_mul_ps(v_v_pw, v_w), _mm256_set1_ps(1.0f / 65536.0f));

                    __m256i v_colors = texture.sample8(v_u, v_v);

                    // Masked store for depth and color
                    // We need to use _mm256_maskstore_ps/epi32 but they take a sign-bit mask
                    // Alternatively, loop for now or use _mm256_blendv_epi8 if we had the destination loaded
                    // Since we want to update only passed pixels:
                    for (int i = 0; i < 8; ++i) {
                        if (final_mask & (1 << i)) {
                            app.color_buffer.data[pixel_idx + i] = reinterpret_cast<uint32_t*>(&v_colors)[i];
                            app.depth_buffer.data[pixel_idx + i] = reinterpret_cast<int32_t*>(&v_new_depth)[i];
                        }
                    }
                };
                Rasterizer::draw_triangle<Varying>(vs, fs, screen_aabb, app.thread_pool.get());
            }
        }
        app.send_framebuffer();
    }
    return 0;
}
