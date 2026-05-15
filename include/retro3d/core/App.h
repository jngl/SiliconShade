#ifndef INC_3D_TEST_APP_H
#define INC_3D_TEST_APP_H

#include <retro3d/core/Memory.h>
#include <retro3d/core/ThreadPool.h>
#include <memory>
#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class App {
public:
    static constexpr int32_t WIN_WIDTH = 1024;
    static constexpr int32_t WIN_HEIGHT = 768;

    App(Arena& arena);
    ~App();

    // Disable copy
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool init_success() const { return m_init_success; }
    bool is_running() const { return m_is_running; }
    void stop() { m_is_running = false; }

    void process_input();
    void send_framebuffer();

    View<uint32_t> color_buffer;
    View<int32_t> depth_buffer;
    std::unique_ptr<ThreadPool> thread_pool;

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_color_buffer_texture = nullptr;
    bool m_is_running = false;
    bool m_init_success = false;
};

#endif //INC_3D_TEST_APP_H
