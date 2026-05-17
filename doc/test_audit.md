# Test Pyramid Audit — Retro3D Engine

**Date:** 2026-05-17 (updated after Section 0 fixes)
**Auditor:** test-pyramid-engineer agent
**Scope:** Full codebase — engine library, tests, examples

---

## Executive Summary

The test suite now consists of **6 unit test binaries, 12 test cases, and ~22 subcases**. There are still no integration tests and no end-to-end tests. Estimated overall coverage has risen from ~18% / ~14% to approximately **28% line / ~22% branch**, driven by the addition of `ModelTests` and `TextureTests` and the new subcases in `MemoryTests` and `ThreadTests`. All 8 Section-0 bugs have been resolved. Two new issues were found during re-read: a destructor mismatch in `Texture.cpp` (low severity, latent) and a missing left-boundary pixel mask in `Rasterizer.h` (medium severity). The multithreaded rasterization path remains unexercised by any test.

---

## Bugs Found

### Bug 1 — `Texture` destructor always calls `stbi_image_free` regardless of allocation path

**File:** `engine/src/render/Texture.cpp:40-44`
**Severity:** Low (latent — safe today, fragile by design)

The file-load constructor allocates with `stbi_load` (internally `malloc`). The in-memory constructor (`Texture(const unsigned char*, int, int)`) allocates with `std::malloc`. The destructor calls `stbi_image_free(data)` unconditionally, which maps to `free()` by default and is therefore safe as long as the stb default allocator is never overridden. However, if a project-wide custom `STBI_MALLOC` / `STBI_FREE` is ever defined (e.g., to use an arena), the in-memory path will call the wrong deallocator. The class should track which path allocated `data` (a `bool owns_stbi` flag, or a separate `free_fn` pointer) so both constructors have a matching destructor.

---

### Bug 2 — Rasterizer tile loop: no left-boundary pixel mask

**File:** `engine/include/retro3d/render/Rasterizer.h:100, 138-140`
**Severity:** Medium

The tile x-loop starts at `min_x & ~(TILE_SIZE - 1)`, which rounds `min_x` down to the nearest tile boundary. This means the first tile may start up to 7 pixels left of `min_x` (the clamped AABB edge). A right-boundary mask is applied when `tx + 7 > max_x` (line 138), but no equivalent left-boundary mask is applied when `tx < min_x`. For a triangle whose leftmost vertex is inside the AABB but whose tile starts outside it, pixels at columns `tx` through `min_x - 1` are only screened by the edge-function sign. If such a pixel lies inside the triangle (edge functions all non-negative) and also outside the screen AABB, the fragment shader is invoked with an x-coordinate below `screen_aabb.min.x`, potentially writing to an out-of-bounds buffer position. The existing AABB Clipping test (`tests/test_engine.cpp`) only counts pixels written within the expected region; it does not assert that no pixels are written outside it. **Fix:** add a left-boundary mask symmetrically to the existing right-boundary mask:

```cpp
if (tx < min_x) {
    int left_mask = ~((1 << (min_x - tx)) - 1) & 0xFF;
    mask &= left_mask;
}
```

---

## Test Suite Evaluation

### MathTests (`tests/test_math.cpp`)

Tests present: `Vec3 Operations` (Addition, Subtraction, Dot Product, Cross Product), `Mat4 Operations` (Identity, Rotation Y at 90°).

Gaps remaining:
- Float equality still uses bare `==` for integer-valued results — `doctest::Approx` is only applied for dot product and the matrix identity/rotation subcases. New tests should consistently use `doctest::Approx`.
- `rotate_y` tested at 90° only; 0°, 180°, 360°, and negative angles not covered.
- `Vec3f::cross` tested for one perpendicular pair; no parallel vectors, self-cross, or zero inputs.
- `Vec3f::operator*` (scalar multiply) not tested.
- `Mat4::transform` with a non-zero translation row not tested.
- Integer types `Vec2`, `Vec3`, `Vec4` — zero tests.
- `AABB` functions (`aabbOfTriangle`, `aabbIntersect`) — zero tests.

**Estimated coverage: ~30% of Math.h** (unchanged from prior audit — no new math tests added)

---

### MemoryTests (`tests/test_memory.cpp`)

Tests present: `Arena Memory Allocator` (Basic Allocation, Arena Clear/Reset), `Arena overflow aborts` (new — uses `SIGABRT` + `setjmp/longjmp`).

The overflow abort test correctly validates FIX-01: it fills the arena to capacity, then asserts that a one-byte overflow triggers `SIGABRT` even in Release builds.

Gaps remaining:
- Alignment guarantee not tested (no check that returned pointers satisfy `alignof(T)` for `double` or `__m256`).
- Zeroing guarantee not tested (allocate, clear, re-allocate in same region, verify bytes are zero).
- Placement-new branch (non-trivially-default-constructible types) not tested.
- `SystemMemory` pointer alignment (32-byte) not tested.

**Estimated coverage: ~40% of Memory.h / Memory.cpp** (up from ~30%; overflow path now exercised)

---

### ThreadTests (`tests/test_threads.cpp`)

Tests present: `ThreadPool Operations` (Stress Test Task Execution and Wait, `wait_all` returns only after all results are written — new).

The new `wait_all` subcase (FIX-04) enqueues 16 tasks that each sleep 2 ms and write a distinct result, then asserts all 16 values are present after `wait_all` returns. This validates the mutex-protected `active_tasks` decrement and the `wait_condition` signal path.

Gaps remaining:
- Only one thread count tested (4). No `ThreadPool(1)`, `ThreadPool(0)`, or `hardware_concurrency`.
- `wait_all` on an empty queue not tested.
- Destructor with in-flight tasks not tested.
- Task exceptions (`std::terminate` path) not tested.

**Estimated coverage: ~45% of ThreadPool** (up from ~35%; completion invariant now directly tested)

---

### EngineTests (`tests/test_engine.cpp`)

Tests present: `Rasterizer Operations` (Interpolation Accuracy, Depth Buffer, AABB Clipping, Degenerate Triangles), `Performance Benchmark` (skipped).

`is_color_near` now correctly casts `uint8_t` operands to `int` before subtraction (FIX-02), so existing assertions in the Depth Buffer and AABB Clipping subcases are now reliable.

Gaps remaining:
- **Multithreaded `draw_triangle` path never called** — the `if (pool)` branch remains completely absent from all tests.
- Clockwise-wound triangle (vertex-swap path) not tested.
- `edge_function` and `interpolate8` not unit tested independently.
- `Draw::draw_clear_buffer` and `draw_clear_depth_buffer` — 0% coverage.
- AABB pixel-count assertion (`51 * 51`) is brittle — breaks if pixel-center convention changes.
- Left-boundary overshoot not detected (see Bug 2 above).

**Estimated coverage: ~40% of Rasterizer.h, 0% of Draw.cpp** (unchanged)

---

### ModelTests (`tests/test_model.cpp`) — NEW

Tests present: `Model bounding sphere radius` (center is at the AABB midpoint, radius equals distance from center to corner, radius is not the AABB largest dimension).

The test writes a unit-cube OBJ to `/tmp`, loads it, and asserts:
1. `get_center()` returns `(0, 0, 0)`.
2. `get_bounding_sphere_radius()` returns `sqrt(3)/2 ≈ 0.866`.
3. The radius is strictly less than 1.0, ruling out the old diameter formula.

This directly validates FIX-05 and catches any regression to the buggy formula.

Gaps remaining:
- `Model::load` with an invalid path (returns `false`) not tested.
- `Model::load` with a face missing texture coordinates (idx.texcoord_index < 0 branch) not tested.
- A model with a single vertex (degenerate mesh — `radius == 0.0f`) not tested.
- `Model::load` with a non-triangulated face (more than 3 vertices per face) not tested.
- `meshes`, `vertices`, and `indices` counts not asserted.

**Estimated coverage: ~35% of Model.cpp** (new; previously 0%)

---

### TextureTests (`tests/test_texture.cpp`) — NEW

Tests present:
- `Texture::is_valid` (null data from failed file load, zero width, zero height, valid 1×1 texture).
- `Texture::sample returns sentinel on invalid texture` (null-data texture, zero-dimension texture).
- `Texture::sample8 returns sentinel on invalid texture` (null-data texture, zero-dimension texture).
- `Texture::sample round-trips bytes from in-memory texture` (2×1 texture, two known pixel values).

The `is_valid` tests validate FIX-06: a texture with `width == 0` or `height == 0` now returns `false` from `is_valid()`, and both `sample` and `sample8` return the `0xFFFFFFFF` sentinel instead of reaching the gather path. The in-memory constructor (`Texture(const unsigned char*, int, int)`) is exercised by four subcases.

The `sample` round-trip test verifies the byte layout: two known `uint32_t` values are stored in an 8-byte buffer, and `sample` at `u = 0.0` and `u = 0.75` must return the correct values unchanged. Note that this test intentionally confirms there is *no* RGBA-to-ARGB swizzle in the in-memory path (unlike the file-load path, which swizzles in the constructor).

Gaps remaining:
- `sample8` positive path (valid texture, known UV inputs, expected pixel outputs) not tested.
- RGBA-to-ARGB swizzle applied by the file-load constructor not tested (would require a real PNG on disk or an injected swizzle test).
- Texture with `width > 0` but `data == nullptr` (defensive case) not tested — the current in-memory constructor silently leaves `data = nullptr` if `malloc` fails; `is_valid()` would return `false` but this path is not exercised.
- `stbi_image_free` vs `free` mismatch on in-memory path (see Bug 1 above) not tested.

**Estimated coverage: ~55% of Texture.cpp / Texture.h** (new; previously 0%)

---

## Untested Modules

| Module | Risk | Reason |
|---|---|---|
| `Rasterizer` + `ThreadPool` (MT path) | **Critical** | Most performance-critical path; tile-seam bugs, partial writes |
| `Input` | High | Two-frame key state machine; `clear_frame_data` contract |
| `Model::load` error paths | High | Invalid OBJ, missing texcoords, degenerate meshes |
| `Engine::run` lifecycle | High | `init_success` early-exit, `on_update`/`on_render` call order |
| `Draw` | Medium | Both clear functions entirely untested |
| `App` | Medium | SDL2 integration, buffer dimensions |
| `Texture::sample8` (valid path) | Medium | AVX2 gather on a valid texture never exercised |

---

## Coverage Summary

| Module | Line | Branch |
|---|---|---|
| `Math.h` | 35% | 25% |
| `Memory.h` / `.cpp` | 45% | 35% |
| `ThreadPool.cpp` / `.h` | 50% | 45% |
| `Input.cpp` / `.h` | 0% | 0% |
| `Draw.cpp` | 0% | 0% |
| `Texture.cpp` / `.h` | 55% | 40% |
| `Model.cpp` / `.h` | 35% | 25% |
| `Rasterizer.h` | 55% | 40% |
| `Engine.cpp` | 0% | 0% |
| `App.cpp` | 0% | 0% |
| **Overall** | **~28%** | **~22%** |

---

## Test Pyramid Assessment

| Layer | Target | Actual |
|---|---|---|
| Unit | ~70% of tests | 100% (6 files, 12 cases, ~22 subcases) |
| Integration | ~20% of tests | 0 |
| E2E / System | ~10% of tests | 0 |

The pyramid remains flat. The number of unit test binaries has grown from 4 to 6 and case count has roughly doubled, but integration and E2E layers are still entirely absent.

---

## Concrete Test Recommendations

### Priority 1 — Fix the two new bugs found during re-read

1. **Bug 2 — Left-boundary mask in Rasterizer tile loop** (`Rasterizer.h:100`): add a left-clamp mask mirroring the existing right-clamp mask (see fix above). Extend the AABB Clipping test to assert that zero pixels are written *outside* the clip region, not just that 51×51 pixels are written inside it.
2. **Bug 1 — Texture destructor mismatch** (`Texture.cpp:40-44`): track whether `data` was allocated by stbi or by `malloc` (e.g., a `bool owns_stbi` member) and call the matching deallocator in the destructor.

### Priority 2 — Complete ThreadPool unit tests (task P1-08)

- Add `ThreadPool(1)` and `ThreadPool(0)` construction tests.
- Add `wait_all` on an empty queue (should return immediately).
- Add destructor-with-in-flight-tasks test (construct, enqueue a slow task, destroy — should not hang or crash).
- Re-run thread tests under TSan (`-DENABLE_TSAN=ON`) to confirm the FIX-03 race is gone.

### Priority 3 — Complete Math unit tests (task P1-08)

- Add `aabbOfTriangle` (degenerate triangle, negative coords).
- Add `aabbIntersect` (non-overlapping AABB pair, touching-edge pair).
- Add `Mat4` translation: apply a translation matrix to a point and verify offset.
- Add `Vec3f::cross` with parallel vectors (expect zero result) and self-cross.
- Add `rotate_y` at 0°, 180°, and 360°.
- Replace bare `==` with `doctest::Approx` in all float comparisons.

### Priority 4 — Complete Memory unit tests (task P1-08)

- Add alignment test: allocate `double` and `__m256` and assert pointer alignment against `alignof(double)` and 32.
- Add zeroing test: allocate, populate with non-zero data via raw pointer cast, call `arena.clear()`, re-allocate in same region, assert bytes are zero.
- Add `SystemMemory` alignment test: assert `mem.data` is 32-byte aligned.

### Priority 5 — Extend Model and Texture tests

- `ModelTests`: load invalid path, load face with no texcoords, load a single-vertex model.
- `TextureTests`: exercise `sample8` with a known valid 4×4 texture and verify pixel values; add a test that constructs a zero-dimension texture with `Texture(pixels, 0, 0)` and asserts `is_valid()` is false with no crash.

### Priority 6 — Integration test binary (`tests/test_integration.cpp`)

1. **Single-threaded vs multi-threaded rasterization**: draw an identical triangle with `pool = nullptr` and with a 4-thread `ThreadPool`; compare all pixels byte-for-byte. This is the only way to catch tile-seam rendering bugs.
2. **Texture + Rasterizer**: construct a 4×4 in-memory texture, draw a textured triangle using `Texture::sample8`, verify sampled pixel colours at known positions.
3. **Arena + typed lifecycle**: allocate, populate, reset, re-allocate in same region, verify zeroing guarantee.

### Priority 7 — E2E / headless tests

- Set `SDL_VIDEODRIVER=offscreen` in CTest test properties to enable SDL2 tests without a display.
- Add a headless `Engine` subclass or `HEADLESS` compile flag to test the `run()` lifecycle without a window.

### Priority 8 — Build / CI improvements (tasks CI-01, CI-02, CI-03)

- No CI configuration exists. Add a CI job that builds with `-DENABLE_ASAN=ON` and runs all CTest targets on every pull request (task CI-01).
- Add a CMake `coverage` target (`-fprofile-arcs -ftest-coverage` + `lcov`) to emit an HTML report and track coverage over time (task CI-02).
- Move the skipped benchmark from `doctest::skip(true)` to a `#ifdef ENABLE_BENCHMARKS` guard controlled by a CMake option `-DENABLE_BENCHMARKS=ON` (task CI-03).

---

## Gaps & Recommendations Summary

The eight Section-0 bugs are resolved and validated by new tests. The codebase is in a demonstrably safer state: the arena overflow guard fires in Release, `ThreadPool` synchronisation is race-free, `Texture::is_valid()` correctly rejects zero-dimension textures, and `Model::get_bounding_sphere_radius()` returns a geometrically correct value.

Two new issues were found:

- The `Texture` destructor always calls `stbi_image_free` regardless of whether `data` was allocated by stbi or by `std::malloc`. This is safe today but will become heap corruption if a custom stb allocator is ever configured.
- The rasterizer tile loop applies a right-boundary pixel mask but no left-boundary mask, meaning pixels left of `screen_aabb.min.x` can be emitted by the fragment shader when the first tile of a row starts to the left of the AABB. This is a real out-of-bounds write risk on screens where `min_x` is not tile-aligned.

The structural gaps that remain before Phase 1 can be considered solid are:

1. The multithreaded `draw_triangle` path has never been run by any test. A tile-seam pixel difference between the single-threaded and multi-threaded paths would be invisible until a visible rendering artifact is noticed manually.
2. `Input`, `Engine::run`, `Draw`, and `App` have 0% coverage. These are medium-to-high risk modules for lifecycle and state-machine bugs.
3. Integration and E2E test layers are entirely absent. The pyramid will remain fragile until at least a single integration test validates the ST/MT rasterizer parity.
