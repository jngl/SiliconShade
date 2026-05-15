#ifndef INC_RETRO3D_ENGINE_H
#define INC_RETRO3D_ENGINE_H

#include <retro3d/core/App.h>
#include <retro3d/core/Input.h>

class Engine {
public:
    Engine(Arena& arena);
    virtual ~Engine();

    void run();

protected:
    virtual void on_init() {}
    virtual void on_update(float dt) = 0;
    virtual void on_render() = 0;

    App m_app;
    Input m_input;
    Arena& m_arena;
};

#endif //INC_RETRO3D_ENGINE_H
