#include <retro3d/core/App.h>
#include <SDL2/SDL.h>
#include <cstdio>
#include <thread>

App::App(Arena& arena) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
        return;
    }

    m_window = SDL_CreateWindow("3D Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, 0);
    if (!m_window) {
        std::fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
        return;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
        return;
    }

    m_color_buffer_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        WIN_WIDTH,
        WIN_HEIGHT
    );
    if (!m_color_buffer_texture) {
        std::fprintf(stderr, "Error creating texture: %s\n", SDL_GetError());
        return;
    }

    color_buffer = arena.allocate<uint32_t>(WIN_WIDTH * WIN_HEIGHT);
    depth_buffer = arena.allocate<int32_t>(WIN_WIDTH * WIN_HEIGHT);
    
    uint32_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    thread_pool = std::make_unique<ThreadPool>(num_threads);

    m_is_running = true;
    m_init_success = true;
}

App::~App() {
    if (m_color_buffer_texture) SDL_DestroyTexture(m_color_buffer_texture);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void App::process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_is_running = false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_is_running = false;
            }
        }
    }
}

void App::send_framebuffer() {
    SDL_UpdateTexture(
        m_color_buffer_texture,
        NULL,
        color_buffer.data,
        (int)(WIN_WIDTH * sizeof(uint32_t))
    );

    SDL_RenderCopy(m_renderer, m_color_buffer_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
}
