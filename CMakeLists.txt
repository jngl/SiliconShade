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

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled at the same time")
endif()

# --- Common Compiler Flags ---
set(COMMON_FLAGS -Wall -Wextra -mavx2 -O3 -march=native)

# --- Engine & Examples ---
add_subdirectory(engine)
add_subdirectory(examples)

# --- Tests ---
enable_testing()

macro(add_retro_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE Retro3D doctest::doctest)
    add_test(NAME ${NAME} COMMAND ${NAME})
endmacro()

add_retro_test(MathTests tests/test_math.cpp)
add_retro_test(MemoryTests tests/test_memory.cpp)
add_retro_test(ThreadTests tests/test_threads.cpp)
add_retro_test(EngineTests tests/test_engine.cpp)
add_retro_test(ModelTests tests/test_model.cpp)
