---
name: project-retro3d-architecture
description: Core architectural facts about the Retro3D engine — language, rendering approach, established patterns, and key subsystem ownership model
metadata:
  type: project
---

Engine name: Retro3D (project dir: SiliconShade). C++20, CMake 3.28+, static lib `libRetro3D.a`.

**Platform stack:** SDL2 for windowing/display; stb_image for textures; tinyobjloader for OBJ models. All via FetchContent. No GPU — pure software rasterizer.

**Rendering:** AVX2 SIMD tile-based rasterizer in `Rasterizer.h` (header-only template). Tile size = 8. Fixed-point subpixel coordinates (SUBPIXEL_SHIFT = 4). Template shader protocol: `draw_triangle<TVarying>(VS, FS, AABB, ThreadPool*)`. Fragment shaders receive 8-wide SIMD batches (`__m256`).

**Memory model:** Single `Arena` (bump allocator) wrapping a `SystemMemory` (32-byte aligned malloc). Arena is passed by reference into `Engine` and `App`. Color + depth buffers are arena-managed. All asset data (`Texture` via stbi_load, `Model` via std::vector) is heap-managed outside the arena.

**Game loop pattern:** `Engine` base class with pure-virtual `on_update(float dt)` and `on_render()`, plus defaulted `on_init()`. Game subclasses override these. Arena must outlive the Engine instance.

**Key modules:**
- `core/`: App (SDL window + buffers + thread pool), Engine (game loop), Memory (Arena+View), Math (Vec2/3/4f, Mat4, AABB, Triangle), ThreadPool, Input, Basic (macros)
- `render/`: Rasterizer.h (template header), Draw (buffer clear), Model (OBJ loader), Texture (stb_image + sample8 AVX2)
- `external/`: stb and tinyobjloader impl TUs (isolation from public API — correct)

**Roadmap (PLAN.md):** 6 phases toward Half-Life-style retro FPS. Phase 1: math lib, arena markers, resource manager, camera. Phase 2: fixed-function renderer, ECS, 2D UI. Phase 3: BSP. Phase 4: MD2, physics. Phase 5: HUD. Phase 6: audio.

**Why:** Architecture audit conducted 2026-05-17 to establish baseline before Phase 1 work begins.
**How to apply:** Use when reasoning about what exists vs what the roadmap requires; ground recommendations in the established patterns rather than proposing greenfield rewrites.
