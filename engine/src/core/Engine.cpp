#include <retro3d/core/Engine.h>
#include <chrono>

Engine::Engine(Arena& arena) : m_app(arena), m_arena(arena) {
}

Engine::~Engine() {
}

void Engine::run() {
    on_init();
    
    if (!m_app.init_success()) return;

    auto last_time = std::chrono::high_resolution_clock::now();

    while (m_app.is_running()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        m_app.process_input(m_input);
        
        on_update(dt);
        on_render();

        m_app.send_framebuffer();
    }
}
