#include <retro3d/core/Engine.h>
#include <retro3d/core/Basic.h>
#include <retro3d/core/Memory.h>
#include <retro3d/render/Draw.h>
#include <retro3d/render/Model.h>
#include <retro3d/render/Texture.h>
#include <retro3d/render/Rasterizer.h>
#include <iostream>
#include <vector>

struct Varying {
    int32_t u_pw, v_pw;
};

struct TransformedVertex {
    Vec3f view_pos;
    Rasterizer::VSOutput<Varying> vs_out;
};

class ChestViewer : public Engine {
public:
    ChestViewer(Arena& arena) : Engine(arena), m_texture("assets/chest/texture.png") {
        if (!m_model.load("assets/chest/Chest.obj")) {
            std::cerr << "Failed to load model" << std::endl;
        }
        if (!m_texture.is_valid()) {
            std::cerr << "Failed to load texture" << std::endl;
        }
        m_screen_aabb = {{0, 0}, {App::WIN_WIDTH - 1, App::WIN_HEIGHT - 1}};
    }

protected:
    void on_update(float dt) override {
        m_rotation += 1.0f * dt;
    }

    void on_render() override {
        draw_clear_buffer(m_app.color_buffer, 0xFF151515);
        draw_clear_depth_buffer(m_app.depth_buffer, 0);

        Vec3f center = m_model.get_center();
        float scale = 400.0f / (m_model.get_bounding_sphere_radius() > 0 ? m_model.get_bounding_sphere_radius() : 1.0f);

        Mat4 model_mat = Mat4::rotate_y(m_rotation);

        for (const auto& mesh : m_model.get_meshes()) {
            m_vertex_cache.resize(mesh.vertices.size());

            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                const Vertex& v = mesh.vertices[i];
                Vec3f p = (v.position - center) * scale;
                Vec3f view_p = model_mat.transform(p);
                view_p.z += 600;

                m_vertex_cache[i].view_pos = view_p;
                
                float f = 800.0f / view_p.z;
                auto& out = m_vertex_cache[i].vs_out;
                out.position = { (int32_t)(view_p.x * f + (App::WIN_WIDTH / 2)), (int32_t)(-view_p.y * f + (App::WIN_HEIGHT / 2)), 0, 0 };
                out.position.x <<= Rasterizer::SUBPIXEL_SHIFT;
                out.position.y <<= Rasterizer::SUBPIXEL_SHIFT;
                out.inv_w = (int32_t)((1.0f / view_p.z) * (1 << 24));
                out.varying.u_pw = (int32_t)((v.texcoord.x * (float)out.inv_w));
                out.varying.v_pw = (int32_t)((v.texcoord.y * (float)out.inv_w));
            }

            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                const TransformedVertex& tv0 = m_vertex_cache[mesh.indices[i]];
                const TransformedVertex& tv1 = m_vertex_cache[mesh.indices[i+1]];
                const TransformedVertex& tv2 = m_vertex_cache[mesh.indices[i+2]];

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
                    
                    __m256 v_inv_w = Rasterizer::interpolate8(_mm256_set1_ps((float)v0.inv_w), 
                                                            _mm256_set1_ps((float)v1.inv_w), 
                                                            _mm256_set1_ps((float)v2.inv_w), b0, b1, b2);

                    int32_t pixel_idx = y * App::WIN_WIDTH + x;
                    __m256i v_current_depth = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&m_app.depth_buffer.data[pixel_idx]));
                    __m256i v_new_depth = _mm256_cvtps_epi32(v_inv_w);
                    __m256i v_depth_mask = _mm256_cmpgt_epi32(v_new_depth, v_current_depth);
                    
                    int final_mask = mask & _mm256_movemask_ps(_mm256_castsi256_ps(v_depth_mask));
                    if (final_mask == 0) return;

                    __m256 v_u_pw = Rasterizer::interpolate8(_mm256_set1_ps((float)v0.varying.u_pw),
                                                           _mm256_set1_ps((float)v1.varying.u_pw),
                                                           _mm256_set1_ps((float)v2.varying.u_pw), b0, b1, b2);
                    
                    __m256 v_v_pw = Rasterizer::interpolate8(_mm256_set1_ps((float)v0.varying.v_pw),
                                                           _mm256_set1_ps((float)v1.varying.v_pw),
                                                           _mm256_set1_ps((float)v2.varying.v_pw), b0, b1, b2);

                    __m256 v_u = _mm256_div_ps(v_u_pw, v_inv_w);
                    __m256 v_v = _mm256_div_ps(v_v_pw, v_inv_w);

                    __m256i v_colors = m_texture.sample8(v_u, v_v);

                    for (int i = 0; i < 8; ++i) {
                        if (final_mask & (1 << i)) {
                            m_app.color_buffer.data[pixel_idx + i] = reinterpret_cast<uint32_t*>(&v_colors)[i];
                            m_app.depth_buffer.data[pixel_idx + i] = reinterpret_cast<int32_t*>(&v_new_depth)[i];
                        }
                    }
                };
                Rasterizer::draw_triangle<Varying>(vs, fs, m_screen_aabb, m_app.thread_pool.get());
            }
        }
    }

private:
    Model m_model;
    Texture m_texture;
    float m_rotation = 0;
    std::vector<TransformedVertex> m_vertex_cache;
    AABB m_screen_aabb;
};

int main() {
    SystemMemory app_memory(GIGABYTES(1));
    Arena app_arena(app_memory);

    ChestViewer viewer(app_arena);
    viewer.run();

    return 0;
}
