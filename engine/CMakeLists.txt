# --- Library Target (The Core Engine) ---
add_library(Retro3D STATIC
    src/core/App.cpp
    src/core/Engine.cpp
    src/core/Input.cpp
    src/core/Memory.cpp
    src/core/ThreadPool.cpp
    src/render/Draw.cpp
    src/render/Model.cpp
    src/render/Texture.cpp
    src/external/stb_impl.cpp
    src/external/tinyobj_impl.cpp
)

target_include_directories(Retro3D 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
        ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(Retro3D PUBLIC SDL2::SDL2 m)
target_compile_options(Retro3D PUBLIC ${COMMON_FLAGS})

# Sanitizers Configuration
if(ENABLE_ASAN)
    target_compile_options(Retro3D PUBLIC -fsanitize=address)
    target_link_options(Retro3D PUBLIC -fsanitize=address)
endif()
if(ENABLE_TSAN)
    target_compile_options(Retro3D PUBLIC -fsanitize=thread)
    target_link_options(Retro3D PUBLIC -fsanitize=thread)
endif()
if(ENABLE_UBSAN)
    target_compile_options(Retro3D PUBLIC -fsanitize=undefined)
    target_link_options(Retro3D PUBLIC -fsanitize=undefined)
endif()
