# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Build directories: `build/` (make), `cmake-build-debug/` and `cmake-build-release/` (CLion/ninja).

**Sanitizer builds** (mutually exclusive; ASan and TSan cannot coexist):
```bash
cmake .. -DENABLE_ASAN=ON    # AddressSanitizer
cmake .. -DENABLE_TSAN=ON    # ThreadSanitizer
cmake .. -DENABLE_UBSAN=ON   # UndefinedBehaviorSanitizer
```

## Tests

Run all tests via CTest from the build directory:
```bash
cd build && ctest
```

Run a single test binary directly (doctest allows filtering by name):
```bash
./MathTests                          # all math tests
./MathTests -tc="Vec3 Operations"    # single test case
./MemoryTests
./ThreadTests
./EngineTests
```

## Architecture

The engine is a static library (`libRetro3D.a`). Public headers live under `engine/include/retro3d/`.

### Entry point pattern

Subclass `Engine` and override the three virtual hooks:
```cpp
class MyGame : public Engine {
    void on_init() override { /* load assets once */ }
    void on_update(float dt) override { /* game logic */ }
    void on_render() override { /* draw to m_app.color_buffer */ }
};
```
In `main`, allocate a `SystemMemory` block, wrap it in an `Arena`, and pass it to the subclass. See `examples/chest_viewer/main.cpp` for a complete reference.

### Memory model

`Arena` is a linear (bump-pointer) allocator that owns a pre-allocated `SystemMemory` slab (typically 1 GB). **No dynamic allocations (`new`/`malloc`) are permitted inside `on_update` or `on_render`.** `Arena::allocate<T>()` requires `T` to be trivially destructible. `Arena::clear()` resets the pointer in O(1).

### Rendering pipeline

`App` (owned by `Engine`) holds:
- `color_buffer` — `View<uint32_t>` of size `WIN_WIDTH × WIN_HEIGHT` (1024×768, ARGB)
- `depth_buffer` — `View<int32_t>`, using `inv_w` (1/z × 2²⁴) as depth (higher = closer)
- `thread_pool` — `std::unique_ptr<ThreadPool>` for parallel rasterization

Rasterization is done by `Rasterizer::draw_triangle<TVarying>` (header-only template in `engine/include/retro3d/render/Rasterizer.h`). It takes two lambdas — a vertex shader `VS(int) → VSOutput<TVarying>` and a fragment shader `FS(x, y, mask, b0, b1, b2, v0, v1, v2)` — plus a screen AABB and an optional `ThreadPool*`.

**Fixed-point positions**: screen-space X/Y in `VSOutput::position` must be pre-shifted left by `Rasterizer::SUBPIXEL_SHIFT` (4 bits). The fragment shader receives 8 pixels at a time via AVX2 (`__m256`) barycentric coordinates.

### Math types

Two distinct families:
- `Vec2 / Vec3 / Vec4` — `int32_t`, used by the rasterizer (screen-space fixed-point)
- `Vec2f / Vec3f / Vec4f / Mat4` — `float`, used for 3D transforms

Use `Rasterizer::interpolate8()` for AVX2 barycentric interpolation of `__m256` lanes.

## Conventions

- **Naming**: `PascalCase` for classes/structs, `snake_case` for methods and functions.
- **C++ standard**: C++20. Prefer `std::` utilities except in hot-path code where they hurt performance.
- **AVX2**: All new compute-intensive functions (transforms, rasterization, physics) must include or plan an AVX2 path and maintain scalar/SIMD parity tested by unit tests.

## Definition of Done

A task is complete only when:
1. A unit test is added or updated with meaningful coverage.
2. The code passes `AddressSanitizer` with zero errors.
3. `PLAN.md` is updated to check off completed items.
