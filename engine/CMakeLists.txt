# --- Library Target (The Core Engine) ---
file(GLOB_RECURSE ENGINE_SOURCES 
    "src/core/*.cpp"
    "src/render/*.cpp"
    "src/external/*.cpp"
)

add_library(Retro3D STATIC ${ENGINE_SOURCES})

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
