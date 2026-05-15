cmake_minimum_required(VERSION 3.28)
project(Retro3DEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(SDL2 REQUIRED)

include(FetchContent)

# doctest
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest
  GIT_TAG        v2.5.2
)

# stb (stb_image)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)

# tinyobjloader
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        release
)

FetchContent_MakeAvailable(doctest stb tinyobjloader)

# --- Options ---
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# --- Common Compiler Flags ---
set(COMMON_FLAGS -Wall -Wextra -mavx2 -O3 -march=native)

# --- Library Target (The Core Engine) ---
add_library(Retro3D STATIC
    src/core/App.cpp
    src/core/Memory.cpp
    src/core/ThreadPool.cpp
    src/render/Draw.cpp
    src/render/Model.cpp
    src/render/Texture.cpp
    src/stb_impl.cpp
    src/tinyobj_impl.cpp
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
if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled at the same time")
endif()

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

# --- Main Application Target ---
add_executable(Retro3DEngine src/core/main.cpp)
target_link_libraries(Retro3DEngine PRIVATE Retro3D)

# --- Tests ---
enable_testing()

# Helper macro for adding tests
macro(add_retro_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE Retro3D doctest::doctest)
    add_test(NAME ${NAME} COMMAND ${NAME})
endmacro()

add_retro_test(MathTests tests/test_math.cpp)
add_retro_test(MemoryTests tests/test_memory.cpp)
add_retro_test(ThreadTests tests/test_threads.cpp)
add_retro_test(EngineTests tests/test_engine.cpp)
