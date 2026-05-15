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

void App::process_input(Input& input) {
    input.clear_frame_data();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_is_running = false;
        }
        
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            switch (event.key.keysym.sym) {
                case SDLK_w: input.set_key_state(Input::K_W, pressed); break;
                case SDLK_a: input.set_key_state(Input::K_A, pressed); break;
                case SDLK_s: input.set_key_state(Input::K_S, pressed); break;
                case SDLK_d: input.set_key_state(Input::K_D, pressed); break;
                case SDLK_UP: input.set_key_state(Input::K_UP, pressed); break;
                case SDLK_DOWN: input.set_key_state(Input::K_DOWN, pressed); break;
                case SDLK_LEFT: input.set_key_state(Input::K_LEFT, pressed); break;
                case SDLK_RIGHT: input.set_key_state(Input::K_RIGHT, pressed); break;
                case SDLK_ESCAPE: input.set_key_state(Input::K_ESCAPE, pressed); break;
                case SDLK_SPACE: input.set_key_state(Input::K_SPACE, pressed); break;
            }
        }

        if (event.type == SDL_MOUSEMOTION) {
            input.set_mouse_pos(event.motion.x, event.motion.y);
            input.set_mouse_relative(event.motion.xrel, event.motion.yrel);
        }
    }
    
    if (input.is_key_down(Input::K_ESCAPE)) {
        m_is_running = false;
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
