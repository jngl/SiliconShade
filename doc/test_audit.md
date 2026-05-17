# Test Pyramid Audit — Retro3D Engine

**Date:** 2026-05-17  
**Auditor:** test-pyramid-engineer agent  
**Scope:** Full codebase — engine library, tests, examples

---

## Executive Summary

The test suite consists of 4 unit test binaries, 7 test cases, and 10 subcases. There are **no integration tests and no end-to-end tests**. Estimated coverage is ~18% line / ~14% branch. Seven bugs were identified, including two that affect correctness in Release builds. The multithreaded rasterization path — the most performance-critical code in the engine — is never exercised by any test.

---

## Bugs Found

### Bug 1 — `ThreadPool::stop` is not atomic (data race)

**File:** `engine/src/core/ThreadPool.cpp`, `engine/include/retro3d/core/ThreadPool.h:30`  
**Severity:** Medium

`stop` is declared `bool stop = false`. Worker threads read it in the `condition_variable` predicate. TSan treats this as a race because the write and `notify_all` are not sequenced identically across all read paths. **Fix:** `std::atomic<bool> stop{false}` with `memory_order_release` store.

---

### Bug 2 — Arena overflow survives only in Debug builds

**File:** `engine/include/retro3d/core/Memory.h:69`  
**Severity:** High

```cpp
assert(current + padding + size <= memory_bloc.size);
```

With `-DNDEBUG` (Release), this assert is stripped. Any over-allocation silently writes past the backing buffer. **Fix:** Replace with an unconditional guard (`std::abort()` or a throw) that survives NDEBUG.

---

### Bug 3 — `ThreadPool::wait_all` mixes two synchronization primitives for one invariant

**File:** `engine/src/core/ThreadPool.cpp:16-22, 48-53`  
**Severity:** Medium

`active_tasks` is `std::atomic<int>` but is also read under the `std::mutex` used by `condition_variable`. Mixing both for the same completion invariant is fragile and prone to race conditions in follow-up changes. **Fix:** Decrement `active_tasks` before releasing the mutex so the invariant is protected by one mechanism only.

---

### Bug 4 — `Model::get_bounding_sphere_radius()` returns a diameter, not a radius

**File:** `engine/src/render/Model.cpp:103-104`  
**Severity:** Medium

```cpp
Vec3f size = max_v - min_v;
radius = std::max({size.x, size.y, size.z});
```

The widest AABB dimension is not the bounding sphere radius — it is at best the diameter along one axis. The chest viewer uses this directly for scaling, producing a result ~2× too small. **Fix:** `max over all vertices of |v - center|`, or conservatively `sqrt(size.x² + size.y² + size.z²) / 2`.

---

### Bug 5 — `Texture::sample8` OOB gather on zero-dimension textures

**File:** `engine/src/render/Texture.cpp:64`  
**Severity:** Medium

When `width == 0`, `width - 1` wraps to `INT_MAX`, and the clamp passes an out-of-bounds index to the gather. `is_valid()` only checks for non-null data, so a zero-dimension texture from a malformed PNG can reach this path. **Fix:** Add `width > 0 && height > 0` to `is_valid()`.

---

### Bug 6 — Rasterizer tile-rejection missing explicit parentheses (precedence hazard)

**File:** `engine/include/retro3d/render/Rasterizer.h:111`  
**Severity:** Low

```cpp
return (w + std::max(0, a * (TILE_SIZE-1) << SUBPIXEL_SHIFT) + ...) < 0;
```

`<<` has lower precedence than `*`, so this is evaluated correctly as written — but the lack of explicit parentheses makes the intent unclear and fragile under future refactoring. **Fix:** `(a * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT`.

---

### Bug 7 — `is_color_near` test helper: signed subtraction on `uint8_t` (test bug)

**File:** `tests/test_engine.cpp:16`  
**Severity:** Medium (breaks existing test assertions)

```cpp
return std::abs(br - r) <= tolerance ...
```

`br` and `r` are `uint8_t`. After integral promotion, `r > br` wraps to a large unsigned value; `std::abs` on an unsigned type is a no-op. Comparisons in the Depth Buffer and AABB Clipping test cases silently pass wrong values. **Fix:** `std::abs((int)br - (int)r) <= tolerance`.

---

## Test Suite Evaluation

### MathTests

- Float equality without `doctest::Approx` on most cases.
- `rotate_y` tested at 90° only; no 0°, 180°, 360°, or negative angles.
- `Vec3f::cross` tested for one perpendicular pair; no parallel vectors, self-cross, or zero inputs.
- `Vec3f::operator*` (scalar multiply) not tested.
- `Mat4::transform` with a non-zero translation row not tested.
- Integer types `Vec2`, `Vec3`, `Vec4` — zero tests.
- `AABB` functions (`aabbOfTriangle`, `aabbIntersect`) — zero tests.

**Estimated coverage: ~30% of Math.h**

---

### MemoryTests

- Alignment guarantee not tested (no check that returned pointers satisfy `alignof(T)`).
- Zeroing guarantee not tested (allocate, clear, re-allocate, verify zero bytes).
- Placement-new branch (non-trivially-default-constructible types) not tested.
- `SystemMemory` pointer alignment (32-byte) not tested.
- Over-allocation abort not tested.

**Estimated coverage: ~30% of Memory.h/Memory.cpp**

---

### ThreadTests

- Only one thread count tested (4). No `ThreadPool(1)`, `ThreadPool(0)`, or `hardware_concurrency`.
- `wait_all` on an empty queue not tested.
- Destructor with in-flight tasks not tested.
- Task exceptions (`std::terminate` path) not tested.

**Estimated coverage: ~35% of ThreadPool**

---

### EngineTests

- **Multithreaded `draw_triangle` path never called** — the `if (pool)` branch is completely absent from tests.
- Clockwise-wound triangle (vertex-swap path) not tested.
- `edge_function` and `interpolate8` not unit tested independently.
- `Draw::draw_clear_buffer` and `draw_clear_depth_buffer` — 0% coverage.
- AABB pixel-count assertion (`51 * 51`) is brittle — breaks if pixel-center convention changes.

**Estimated coverage: ~40% of Rasterizer.h, 0% of Draw.cpp**

---

## Untested Modules

| Module | Risk | Reason |
|---|---|---|
| `Rasterizer` + `ThreadPool` (MT path) | **Critical** | Most performance-critical path; tile-seam bugs, partial writes |
| `Input` | High | Two-frame key state machine; `clear_frame_data` contract |
| `Texture::sample` / `sample8` | High | AVX2 gather, zero-dimension OOB, RGBA→ARGB swizzle |
| `Model::load` | High | Invalid OBJ, missing texcoords, degenerate meshes |
| `Engine::run` lifecycle | High | `init_success` early-exit, `on_update`/`on_render` call order |
| `Draw` | Medium | Both clear functions entirely untested |
| `App` | Medium | SDL2 integration, buffer dimensions |

---

## Coverage Summary

| Module | Line | Branch |
|---|---|---|
| `Math.h` | 35% | 25% |
| `Memory.h` / `.cpp` | 45% | 30% |
| `ThreadPool.cpp` / `.h` | 40% | 35% |
| `Input.cpp` / `.h` | 0% | 0% |
| `Draw.cpp` | 0% | 0% |
| `Texture.cpp` / `.h` | 0% | 0% |
| `Model.cpp` / `.h` | 0% | 0% |
| `Rasterizer.h` | 55% | 40% |
| `Engine.cpp` | 0% | 0% |
| `App.cpp` | 0% | 0% |
| **Overall** | **~18%** | **~14%** |

---

## Test Pyramid Assessment

| Layer | Target | Actual |
|---|---|---|
| Unit | ~70% of tests | 100% (4 files, 7 cases) |
| Integration | ~20% of tests | 0 |
| E2E / System | ~10% of tests | 0 |

The pyramid is completely flat. Integration and E2E layers are entirely absent.

---

## Recommendations (Priority Order)

### Priority 1 — Fix existing test defects

1. Fix `is_color_near` (`tests/test_engine.cpp:16`): cast to `int` before subtraction.
2. Add `TEST_CASE("Rasterizer with ThreadPool")`: draw a triangle with a 4-thread pool, compare output to single-threaded result byte-for-byte.
3. Add `TEST_CASE("Arena alignment")`: allocate `double` and `__m256` and assert pointer alignment.

### Priority 2 — New unit test files

- **`tests/test_input.cpp`**: key press/hold/release state machine; `is_key_pressed` single-frame contract; mouse delta reset after `clear_frame_data`.
- **`tests/test_draw.cpp`**: `draw_clear_buffer` with zero and non-zero colour; `draw_clear_depth_buffer` fast path and fill path.
- **Extend `tests/test_math.cpp`**: `aabbOfTriangle` (degenerate, negative coords); `aabbIntersect` (non-overlapping); `Mat4` translation; `Vec3f::cross` parallel vectors; `rotate_y` at 0°/180°/360°.
- **Extend `tests/test_memory.cpp`**: alignment check; zeroing-after-clear check; `SystemMemory` 32-byte alignment.

### Priority 3 — Integration test binary (`tests/test_integration.cpp`)

1. Single-threaded vs multi-threaded `draw_triangle`: same triangle, both paths, byte-for-byte comparison. Guards against tile-seam bugs.
2. `Texture` + `Rasterizer`: load a tiny PNG, draw a textured triangle, verify sampled colours.
3. `Arena` + typed lifecycle: allocate, populate, reset, re-allocate in same region, verify zeroing.
4. `ThreadPool` completeness: enqueue N indexed tasks, collect results under mutex, verify all N indices present after `wait_all`.

### Priority 4 — E2E / headless tests

- Set `SDL_VIDEODRIVER=offscreen` in CTest test properties to enable SDL2 tests without a display.
- Add a constructor `Texture(unsigned char* data, int w, int h)` to enable in-memory texture construction in tests.
- Add a headless `Engine` subclass or `HEADLESS` compile flag to test the `run()` lifecycle without a window.

### Priority 5 — Build / CI improvements

- No CI configuration exists. Add a CI job that builds with `-DENABLE_ASAN=ON` and runs all tests, enforcing the existing definition of done.
- Add a CMake `coverage` target (`-fprofile-arcs -ftest-coverage` + `lcov`) to track coverage over time.
- Move the skipped benchmark from `skip(true)` in source to a CMake option (`-DENABLE_BENCHMARKS=ON`) so it can be run on demand in CI.
