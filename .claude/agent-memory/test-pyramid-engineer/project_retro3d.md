---
name: project-retro3d
description: Core facts about the Retro3D engine project — architecture, build, and test infrastructure
metadata:
  type: project
---

Retro3D is a C++20 software rasterizer / retro FPS engine built as a static library (`libRetro3D.a`).

**Build**: CMake 3.28+, FetchContent (doctest v2.5.2, stb, tinyobjloader), SDL2 system-wide. AVX2 mandatory (`-mavx2 -march=native`). ASan/TSan/UBSan flags available as CMake options.

**Architecture**:
- `Engine` base class; subclasses override `on_init()`, `on_update(float dt)`, `on_render()`.
- `Arena` bump-pointer allocator backed by `SystemMemory`; only trivially-destructible types; no dynamic allocation in render loop.
- `Rasterizer::draw_triangle<TVarying>` is a header-only template; takes VS/FS lambdas + optional `ThreadPool*`. Screen-space positions in fixed-point (left-shifted by `SUBPIXEL_SHIFT=4`). TILE_SIZE=8. AVX2 throughout hot path.
- Two math families: `Vec2/Vec3/Vec4` (int32_t, rasterizer) and `Vec2f/Vec3f/Vec4f/Mat4` (float, transforms).
- `ThreadPool`: worker threads share a single `std::queue`, protected by `queue_mutex`. `active_tasks` is `std::atomic<uint32_t>`. `stop` is a plain `bool` (not atomic).

**Test framework**: doctest v2.5.2. Each test binary has its own `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`.
Test binaries: MathTests, MemoryTests, ThreadTests, EngineTests.
Run: `cd build && ctest` or individual binaries.

**Definition of Done**: unit test added/updated + ASan clean + PLAN.md updated.

**Why:** Understanding full scope needed to plan all test layers correctly.
**How to apply:** Use these facts when planning new tests, evaluating coverage gaps, or suggesting refactors.
