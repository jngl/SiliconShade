# Architecture Audit — Retro3D Engine

**Date:** 2026-05-17 (updated — Section 0 fixes applied)
**Auditor:** game-engine-architect agent
**Scope:** Full codebase — engine library, examples, build system, roadmap alignment
**Cross-reference:** See `doc/test_audit.md` for the companion test-pyramid audit.

---

## Executive Summary

Retro3D is a well-scoped, early-stage software rasterizer with a clear vision. The foundation — an arena allocator, a tile-based AVX2 rasterizer, and a template-driven shader interface — is technically solid and demonstrates real performance awareness. The Engine subclass pattern is ergonomic for demos.

All eight Section-0 correctness defects identified in the original audit have been resolved. The arena overflow guard now fires unconditionally in Release builds. `ThreadPool` synchronization is race-free. `Texture::is_valid()` correctly rejects zero-dimension inputs. The bounding sphere radius formula is exact. The CMake build uses explicit source lists.

One new defect was identified during the post-fix test re-read (see `doc/test_audit.md` Bug 2): the rasterizer tile x-loop applies a right-boundary pixel mask but no left-boundary mask, meaning pixels at column positions left of `screen_aabb.min.x` can be emitted by the fragment shader when the first tile of a row starts below the AABB boundary. This is an active out-of-bounds write risk.

Five architectural decisions made at this stage will become increasingly painful as the project moves from Phase 1 toward Phase 3 and Phase 4. The most serious are: the Arena lacks the lifecycle controls the roadmap requires, the per-triangle thread dispatch cannot meet the 60 FPS target at scene scale, the `Model` and `Texture` types are incompatible with the planned resource cache, and the `App` class hard-codes resolution at compile time. None of these require a rewrite — they require targeted, planned refactoring before the next phase begins.

---

## 1. Structural Soundness

### 1.1 Module Dependency Graph

```
[examples]
    └─> Engine (Engine.h / Engine.cpp)
           ├─> App (App.h / App.cpp)
           │     ├─> Memory.h        (Arena, View, SystemMemory)
           │     ├─> ThreadPool.h    (ThreadPool)
           │     └─> Input.h         (Input)
           └─> Input.h

[Rasterizer.h]   (header-only template)
    └─> Math.h
    └─> ThreadPool.h

[Draw.h / Draw.cpp]
    └─> Memory.h

[Model.h / Model.cpp]
    └─> Math.h
    └─> tinyobjloader (PRIVATE, via src/)

[Texture.h / Texture.cpp]
    └─> stb_image (PRIVATE, via src/)
    └─> immintrin.h

[Math.h]         (no engine dependencies — correct)
[Memory.h]       (depends on Math.h — unnecessary coupling, see §1.3)
[Basic.h]        (pure macros — correct)
```

The dependency graph is largely acyclic and layered correctly. Third-party headers (`stb_image.h`, `tiny_obj_loader.h`) are correctly isolated to implementation translation units under `src/external/`, avoiding contamination of the public API. The engine CMakeLists.txt now uses an explicit source list (FIX-08), eliminating the silent-missing-file failure mode.

### 1.2 Separation of Concerns

| Concern | Module | Assessment |
|---|---|---|
| Platform / windowing | `App` | Mixed: handles SDL init, buffer allocation, and thread pool creation — three separate concerns in one class |
| Game loop | `Engine` | Clean: thin shell around `on_init` / `on_update` / `on_render` |
| Rasterization | `Rasterizer.h` | Mixed: rasterizer logic, threading dispatch, and the shader protocol are all in one header |
| Math primitives | `Math.h` | Partially mixed: render-domain types (`Triangle`, `AABB`) are in the same file as generic math types |
| Memory | `Memory.h` | Mixed: `Memory.h` includes `Math.h` — there is no reason a memory allocator needs to know about vectors |
| Input | `Input` | Clean |
| Asset loading | `Model`, `Texture` | Correct but incomplete — no resource lifetime management |

### 1.3 The `Memory.h` → `Math.h` Dependency

`Memory.h` includes `Math.h` at line 4, but nothing in `Memory.h` uses any type from `Math.h`. This is a vestigial include that creates a false coupling. Every translation unit that includes `Memory.h` (which is nearly everything, since `App.h` includes it) is forced to compile `Math.h`. Remove the include from `Memory.h`.

### 1.4 Render-Domain Types in `Math.h`

`Triangle` and `AABB` are render-specific types that live in `Math.h`. As the roadmap adds BSP planes, rays, physics AABBs, and frustums, `Math.h` will grow into an undifferentiated bag of types. Split early:

- `Math.h` — pure numeric primitives: `Vec2`, `Vec3`, `Vec4`, `Vec2f`, `Vec3f`, `Vec4f`, `Mat4`
- `Geometry.h` — spatial structures: `AABB`, `Plane`, `Ray`, `Sphere`, `Frustum`
- `RenderTypes.h` — rasterizer-domain: `Triangle`, subpixel constants

---

## 2. Performance Architecture

### 2.1 The Rasterizer: What Works Well

The tile-based AVX2 rasterizer in `Rasterizer.h` is technically the most mature piece of the engine. Key strengths:

- **Fixed-point subpixel coordinates** with `SUBPIXEL_SHIFT = 4` give correct sub-pixel rasterization without floating-point edge function evaluation.
- **Tile rejection** via the conservative covering test avoids invoking the per-pixel SIMD path for fully-outside tiles. The parenthesization of the tile-rejection expression is now explicit — `(a * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT` — eliminating operator-precedence fragility (FIX-07).
- **Template shader protocol** (`VS`, `FS` callables + `TVarying` parameter) achieves zero-cost abstraction — the compiler inlines both shaders into the loop body, and the varying type is a compile-time parameter that allows SoA-style data access patterns per invocation.
- **Incremental edge stepping** (addition, not recomputation) is the correct approach for inner loops.
- **SIMD gather for texture sampling** (`_mm256_i32gather_epi32`) in `Texture::sample8` correctly processes 8 texels in parallel and is now guarded by `is_valid()` (FIX-06).

### 2.2 The Rasterizer: Critical Performance and Correctness Issues

**Issue 1: Per-triangle thread dispatch (dominant bottleneck at scale)**

```cpp
// Rasterizer.h:166-181
if (pool) {
    // ... enqueue one task per thread stripe ...
    pool->wait_all();
}
```

`wait_all()` is called once per triangle. For a scene with 10,000 triangles, this is 10,000 mutex lock/unlock pairs on `queue_mutex` (for enqueue) plus 10,000 `condition_variable::wait` calls (for the barrier). The target performance budget in `functional_target.md` is 10,000–20,000 visible triangles at 60 FPS. At that count, the synchronization overhead will exceed the rasterization cost for any triangle smaller than ~100 pixels.

The correct model for a software renderer is a **frame-level tile job system**: divide the screen into N×M tiles once per frame, then assign each tile to a worker thread. Each worker independently processes all triangles for its assigned tiles. There is no per-triangle synchronization. This is the architecture used by every production software renderer (Mesa's softpipe, SwiftShader, DOOM 2016's CPU fallback path).

**Issue 2: `std::function<void()>` allocation in the hot path**

`ThreadPool::enqueue` takes a `std::function<void()>`. `std::function` performs a heap allocation for any closure that exceeds the Small Buffer Optimization threshold (typically 24–48 bytes on libstdc++). The lambda captured in `draw_triangle` captures `worker` (itself a large lambda with ~20 captured values) plus two `int32_t` range bounds — this will reliably exceed the SBO. Every triangle that uses the thread pool therefore causes a heap allocation, directly violating the zero-allocation hot path requirement in `GEMINI.md`.

Fix: Replace `std::function` in `ThreadPool` with a fixed-size work item using a type-erased callable stored in-place (e.g., `std::move_only_function<void()>` in C++23, or a manual analog).

**Issue 3: Tile inner-loop y-bounds check**

```cpp
// Rasterizer.h:124-130
for (int32_t y = ty; y < ty + TILE_SIZE; ++y) {
    if (y < min_y || y > max_y) {
        // advance and skip
        continue;
    }
```

This check fires on every row of every tile that partially overlaps the triangle's vertical bounds. For `TILE_SIZE = 8`, up to 7 out of 8 rows may be skipped, making this a nearly unconditional branch in the inner loop. The correct fix is to start the tile iteration at `max(tile_min_y, min_y)` and end at `min(tile_max_y, max_y)` — no per-row check needed.

**Issue 4: Missing left-boundary pixel mask (new — from `doc/test_audit.md` Bug 2)**

```cpp
// Rasterizer.h:100
for (int32_t tx = min_x & ~(TILE_SIZE - 1); tx <= max_x; tx += TILE_SIZE) {
```

The tile x-loop starts at `min_x & ~(TILE_SIZE - 1)`, which rounds `min_x` down to the nearest tile boundary. The first tile may therefore start up to 7 pixels left of `min_x`. A right-boundary mask is applied when `tx + 7 > max_x` (line 138), but there is no equivalent left-boundary mask for when `tx < min_x`. For a triangle whose leftmost vertex is inside the screen AABB but whose tile starts to the left of it, pixels at columns `tx` through `min_x - 1` are screened only by the edge-function sign. If any such pixel is interior to the triangle (all edge functions non-negative), the fragment shader is invoked with an x-coordinate below `screen_aabb.min.x`, potentially writing to an out-of-bounds buffer position.

Fix: apply a symmetric left-boundary mask immediately after the edge-function mask is computed:

```cpp
if (tx < min_x) {
    int left_mask = ~((1 << (min_x - tx)) - 1) & 0xFF;
    mask &= left_mask;
}
```

The existing AABB Clipping test should also be extended to assert that zero pixels are written outside the clip region, not merely that the expected pixel count is written inside it.

**Issue 5: Perspective-divide encoding and precision**

In `chest_viewer/main.cpp`:

```cpp
out.inv_w = (int32_t)((1.0f / view_p.z) * (1 << 24));
out.varying.u_pw = (int32_t)((v.texcoord.x * (float)out.inv_w));
```

`inv_w` is stored as a fixed-point Q8.24 value in an `int32_t`. The barycentric interpolation of `inv_w` in the fragment shader then uses `_mm256_cvtepi32_ps` on the raw integer representation, treating fixed-point bits as a float. The depth comparison (`v_new_depth > v_current_depth`) compares these Q8.24 fixed-point integers, which is correct. But the perspective-correct UV division `u_pw / inv_w` divides two fixed-point values using float arithmetic — the numerator's scale (`1/z * uv`) and denominator's scale (`1/z`) cancel, giving correct UV coordinates. This is subtle and correct, but the invariant needs a comment, because `u_pw = u * inv_w` is not self-documenting. The correctness depends on the fixed-point scale factors cancelling: `(u * inv_w) / inv_w = u`.

### 2.3 Memory Layout: Arena Usage

The arena is allocated at 1 GiB in both examples (`SystemMemory app_memory(GIGABYTES(1))`). The only arena-managed memory is the color buffer and depth buffer in `App::App()` — approximately 3 MiB for 1024×768. The remaining 997 MiB is reserved but unused.

The arena overflow guard (FIX-01) now calls `std::fprintf` + `std::abort()` unconditionally, surviving `-DNDEBUG`. The diagnostic prints the requested size and remaining capacity before aborting.

More importantly, the `Arena::clear()` method resets the entire arena to offset 0. There is no marker or scope system. When the roadmap adds per-frame temporary allocations (Phase 1: "Arena Markers") alongside persistent allocations (loaded models, textures), the current `Arena` design cannot support both in the same arena without flushing the persistent data.

`Model` and `Texture` are not arena-managed — they allocate via `std::vector` and `stbi_load` / `std::malloc`. The "zero allocation in the hot path" property is a half-truth: the render loop is clean, but asset loading and model destruction use the system allocator.

**Note on `Texture` destructor:** The file-load constructor allocates via `stbi_load` (internally `malloc`) and the in-memory constructor (`Texture(const unsigned char*, int, int)`, added by FIX-06) allocates via `std::malloc`. The destructor calls `stbi_image_free(data)` unconditionally. This is safe as long as the default stb allocator is never overridden. If a custom `STBI_MALLOC` / `STBI_FREE` is ever defined, the in-memory path will call the wrong deallocator. The destructor should track the allocation path (a `bool owns_stbi` flag) and call the matching function.

### 2.4 Threading Model Assessment

The `ThreadPool` is a standard work-stealing queue implementation, well-suited for background tasks. The synchronization issues from the original audit have been resolved:

- `stop` is now `std::atomic<bool>` with `memory_order_release` on write and `memory_order_acquire` on read inside the condition-variable predicate (FIX-03). The TSan race is gone.
- `active_tasks` is now a plain `uint32_t` whose reads and writes are all guarded by `queue_mutex` (FIX-04). The mixed-synchronization invariant (atomic + mutex for the same variable) is eliminated.

The threading model for rendering needs to change before Phase 2 (see §2.2 Issue 1). The thread-local arenas specified in Phase 1 of `PLAN.md` are also not supported by the current single-arena design.

---

## 3. Extensibility Against the Roadmap

### 3.1 Phase 1 — Math and Memory

**Math (`Vec3f`, `Mat4`, `Quat`):** `Vec3f` exists. `Mat4` exists with `identity()`, `rotate_y()`, and `transform()`. Missing: full matrix multiply (required for view-projection pipeline), `look_at`, `perspective`, quaternion type, `Vec3f::normalize()`, `Vec3f::length()`. The current `Mat4::transform` drops the W component — it is a 3D affine transform, not a full 4D transform. This works for the current examples but will break once a full MVP pipeline is required.

**Memory Markers:** The `Arena` has `clear()` but no `get_marker()` / `restore_marker()`. This is a Phase 1 deliverable. Without markers, temporary per-frame allocations (e.g., a clipped polygon list during BSP traversal) cannot coexist with persistent allocations in the same arena.

**Pool Allocator:** Not present. Required by Phase 1 for ECS components.

**Resource Cache:** `Model` and `Texture` each own their data directly with no reference counting or external ownership model. The planned `ResourceManager` (Phase 1) needs assets to be shareable and lifetime-managed. The current design requires refactoring both classes to use handles or shared ownership before the resource cache can be built.

`Texture` now has a second constructor `Texture(const unsigned char*, int, int)` for in-memory construction (added by FIX-06). This is a prerequisite for BSP embedded textures and for test isolation. Move semantics are still absent — `Texture(Texture&&)` is not defined, limiting the ResourceManager's ability to store textures in a growable container.

### 3.2 Phase 2 — ECS and Renderer

**ECS:** No ECS exists. The roadmap requires `World`, `Entity`, and component storage. The `Engine` subclass pattern is incompatible with ECS in its current form: `on_render()` is expected to contain imperative mesh-drawing code (as in `chest_viewer`), but a `RenderSystem` needs to iterate component arrays. The `Engine` interface will need an extension point for registering and running systems.

**Dynamic Resolution:** `App` hard-codes `WIN_WIDTH = 1024` and `WIN_HEIGHT = 768` as `static constexpr int32_t` on the class. The color and depth buffers are sized at construction using these constants. The `screen_aabb` in the viewer is also constructed from these constants. Dynamic resolution requires: a runtime-specified buffer size, resizable arena allocations (or a separate allocator for frame buffers), and the AABB/viewport to reflect the current size. This is a non-trivial refactor across at least four files.

**Frustum Clipping:** The rasterizer performs 2D scissoring (AABB intersection at screen space) but no 3D frustum clipping before perspective divide. Triangles that cross the near plane will produce incorrect results (z ≤ 0 in view space leads to a divide by a near-zero or negative value). The `chest_viewer` avoids this because the model is always placed at z = 600. For BSP traversal where the player can approach any world geometry, near-plane clipping is required before the rasterization call.

### 3.3 Phase 3 — BSP

The BSP phase requires:
- **`Plane` type** (not yet in `Math.h`)
- **Polygon clipping** against planes (Sutherland-Hodgman or equivalent)
- **PVS bitset** — a packed bit array indexed by cluster, requiring a memory-efficient container
- **Lightmap atlas** — a large texture with non-power-of-two atlas dimensions

None of these exist, but their absence is expected at Phase 3. The concern is that the rasterizer's current flat triangle API (`draw_triangle<TVarying>`) has no concept of lightmap UV as a second texture coordinate. The `Varying` struct would need a second UV pair, which the template handles automatically — no rasterizer change required, only a new `Varying` definition. This is a genuine extensibility win of the template approach.

### 3.4 Phase 4 — MD2 and Physics

**MD2:** The `Mesh` struct uses `std::vector<Vertex>` and `std::vector<uint32_t>`. MD2 meshes are interleaved index lists with a fixed vertex count per keyframe. The current `Mesh` structure can represent a single MD2 frame, but MD2 animation requires multiple frame buffers and an interpolation pass. The `Model` class would need extension, but the `Mesh` struct is not hostile to this.

**Physics:** BSP clip nodes are a specialized data structure that has nothing to do with the current `Model` / `Mesh` types. The physics system will be an independent module, which is correct.

---

## 4. API Design

### 4.1 Engine Subclass Pattern

```cpp
class MyGame : public Engine {
    void on_update(float dt) override { ... }
    void on_render() override { ... }
};
int main() {
    SystemMemory mem(GIGABYTES(1));
    Arena arena(mem);
    MyGame game(arena);
    game.run();
}
```

**Strengths:**
- The CRTP-free virtual dispatch is simple and correct at the game loop level (one call per frame, cost is negligible).
- The arena-at-construction pattern forces the caller to manage memory lifetime explicitly — the arena must outlive the `Engine`. This is correct ownership semantics, and the examples demonstrate it correctly.
- `m_app`, `m_input`, and `m_arena` are exposed as `protected` members, which gives game subclasses direct access without getters. This is pragmatic for a single-game engine.

**Weaknesses:**
- `on_init()` is a virtual function with a default empty implementation. If a subclass forgets to call a hypothetical base `on_init()` in the future (e.g., when the base class initializes the ECS `World`), there is no enforcement mechanism. Consider making `on_init()` non-virtual in the base and calling a protected `do_init()` virtual instead, or documenting the invariant.
- `on_update` and `on_render` are both pure virtual, requiring every subclass to implement both. The `CryptCrawler` example has a trivial `on_render` that only clears buffers. A default `on_render` that clears the buffers (the overwhelmingly common case) would reduce boilerplate.
- The `Engine` constructor silently falls through if `App::init()` fails — `m_app.init_success()` returns false and `run()` exits immediately. No exception is thrown on init failure. Error reporting on failure is limited to `fprintf(stderr, ...)` inside `App`. This is acceptable for a demo engine but should be documented explicitly.

### 4.2 Arena Allocator Contract

The `Arena::allocate<T>` function has a correct and useful design:
- Alignment is respected.
- Trivially-default-constructible types are zero-initialized via `memset` (not via the default constructor, which is correct).
- Non-trivially-default-constructible types receive placement-new. The `static_assert(std::is_trivially_destructible_v<T>)` correctly prevents arenas from managing types with destructors, which would leak.
- Overflow in Release builds now calls `std::fprintf` + `std::abort()` unconditionally, printing requested size and remaining capacity (FIX-01).

**Remaining gaps in the contract:**
- No `push_scope()` / `pop_scope()` (markers). Calling code cannot separate temporary from persistent allocations in the same arena.
- There is no `allocate_single<T>(args...)` convenience. Call sites read `arena.allocate<T>(1)` and then manually index `.data[0]`, which is error-prone.
- `View<T>` is a raw span with no bounds checking. There is no debug-mode bounds-check path. Adding an `operator[]` with `assert(i < size)` to `View<T>` would catch many indexing bugs at negligible cost.

### 4.3 Rasterizer Template Interface

The template protocol for vertex and fragment shaders is the most architecturally distinctive decision in the engine:

```cpp
template<typename TVarying, typename VS, typename FS>
void draw_triangle(const VS& vs, const FS& fs, const AABB& screen, ThreadPool* pool);
```

**Strengths:**
- Complete monomorphization: the compiler sees the full shader code at instantiation, enabling inlining, constant folding, and vectorization across the shader boundary.
- The `TVarying` type is user-defined, making the attribute system fully general — any interpolatable data can be a varying.
- The fragment shader receives 8-wide SIMD batches (`__m256 b0, b1, b2`) directly, which is the right granularity for AVX2.

**Weaknesses:**
- The entire rasterizer is defined in a header. Every translation unit that includes `Rasterizer.h` (directly or transitively) re-instantiates the full template for every `TVarying` type used in that TU. With a BSP renderer that might use three or four varying layouts (flat color, textured, lightmapped, skybox), build times will grow.
- The fragment shader signature is tied to AVX2 (`__m256`). There is no scalar fallback path for non-AVX2 targets. This is currently acceptable (AVX2 is required per `COMMON_FLAGS = -mavx2`), but it makes the rasterizer non-portable and untestable without AVX2 hardware.
- The `VSOutput<TVarying>` struct stores `Vec4 position` (integer subpixel coords), `int32_t inv_w`, and the `TVarying`. The position and `inv_w` are inseparable from the varying even when a caller only needs vertex positions (e.g., for a shadow or depth-only pass). A split into `VSPosition` and `TVarying` would allow depth-only passes to zero the varying cost.
- `ThreadPool* pool = nullptr` as a defaulted parameter means the single-threaded path is chosen by passing null, and the multithreaded path by passing a pointer. This is implicit — a caller who forgets the `pool` argument silently gets single-threaded rendering. An explicit `enum class Parallelism { Single, Multi }` or a separate overload would make the choice visible at the call site.

---

## 5. Identified Risks

### Risk 1 — Hard-coded resolution blocks dynamic resolution (PLAN.md Phase 2) [HIGH]

`App::WIN_WIDTH` and `App::WIN_HEIGHT` are `static constexpr int32_t`. The SDL texture, color buffer, and depth buffer are all created at these dimensions. The `send_framebuffer()` function passes `WIN_WIDTH * sizeof(uint32_t)` as the row stride. There is no resize path.

To support dynamic resolution, `App` must be refactored to hold `int32_t width, height` as runtime fields, the buffers must be reallocatable (either via arena markers or a separate allocator), and `send_framebuffer` must use the runtime dimensions. This will also require updating the `screen_aabb` and pixel index calculations in every example. **Refactor before Phase 2 begins.**

### Risk 2 — Model and Texture are unmanaged heap resources [HIGH]

`Texture` allocates with `stbi_load` (malloc) for file loads and with `std::malloc` for in-memory construction. `Model` uses `std::vector` (heap). Neither is arena-managed. The planned `ResourceManager` (Phase 1) needs a stable address and a shareable lifetime for each asset. The current design means:
- Two `Texture` objects pointing at the same file will each load and store a separate copy.
- There is no `Texture(Texture&&)` move constructor — move is not available because copy is deleted and no move constructor is defined.

The resource cache will require either handle-based ownership (`ResourceHandle<Texture>` returning a `Texture*` or index into a pool) or shared pointers. **Design the ResourceManager before implementing the Model or Texture cache.**

### Risk 3 — Per-triangle thread dispatch will not meet the 60 FPS target [HIGH]

As analyzed in §2.2, the current threading model synchronizes per triangle. At 10,000 visible triangles, this is 10,000 barrier operations per frame at 60 Hz. Even if each barrier costs only 1 µs (a conservative underestimate for a condition-variable wait), that is 10 ms of synchronization overhead per frame — consuming 60% of the 16.7 ms frame budget before a single pixel is drawn.

**The rendering threading model must be redesigned before Phase 2 adds ECS iteration overhead.** The architecture change is: accumulate all visible triangles into a screen-tile-indexed bucket list, then dispatch one job per screen tile. This is a fundamental redesign of the render loop, not a local patch.

### Risk 4 — Arena lacks the lifecycle controls the roadmap requires [MEDIUM]

Phase 1 lists "Arena Markers for temporary allocations" and "Thread-Local Arenas for parallel rendering" as explicit deliverables. Neither is supportable by the current single flat-cursor `Arena`. Thread-local arenas require independent `Arena` instances wrapping sub-regions of the main `SystemMemory` block. This is straightforward to implement, but the current `Arena` has no `sub_arena(size_t size)` method.

### Risk 5 — Near-plane clipping is absent [MEDIUM]

The rasterizer does not clip triangles against the near plane before perspective divide. Triangles with any vertex at z ≤ 0 in view space will produce a division by zero (or by a near-zero value), placing the projected vertex at a very large screen coordinate. The tile AABB clamp will usually contain the damage, but the barycentric weights will be corrupted, producing incorrect depth and UV values for the surviving pixels.

This is currently safe because `chest_viewer` always places the model at z = 600. It will become an active bug once the player camera can approach any world geometry (Phase 3).

### Risk 6 — `std::function` in ThreadPool violates the zero-allocation hot path rule [MEDIUM]

`GEMINI.md` section 2 states: "No dynamic allocation (new/malloc) is permitted inside the render loop." `ThreadPool::enqueue(std::function<void()>)` performs a heap allocation for closures that exceed the SBO. The `draw_triangle` lambda exceeds the SBO. This violation is already present and will worsen as the captured state grows with ECS system data.

### Risk 7 — Rasterizer tile loop missing left-boundary pixel mask [MEDIUM]

As analyzed in §2.2 Issue 4, the tile x-loop starts below the AABB boundary on non-tile-aligned triangles. The fragment shader can be invoked with `x < screen_aabb.min.x`, writing to a buffer position that is out of bounds relative to the intended clip region. The right-boundary mask is applied correctly; the left-boundary mask is absent. **Fix before adding the multithreaded test path, since the out-of-bounds write is harder to observe in multi-threaded execution.**

---

## 6. Concrete Recommendations

The recommendations are ordered by phase alignment. Items resolved by Section 0 fixes have been removed.

### Rec 1 — Introduce Arena markers (required before Phase 1 completion)

Add `get_marker()` / `restore_to_marker()` to `Arena`:

```cpp
struct ArenaMarker { uint64_t offset; };

ArenaMarker Arena::get_marker() const { return { current }; }
void Arena::restore_to_marker(ArenaMarker m) { current = m.offset; }
```

This enables per-frame temporary allocations (push before frame, restore after) while preserving persistent allocations (models, textures, BSP data) at the base. The existing `clear()` becomes `restore_to_marker({0})`.

### Rec 2 — Separate Math.h into Math.h + Geometry.h (required before Phase 3)

Move `Triangle`, `AABB`, `aabbOfTriangle`, `aabbIntersect` to a new `Geometry.h`. Add Phase 1 types (`Plane`, `Ray`, `Sphere`) to `Geometry.h`. Add `Vec3f::normalize()`, `Vec3f::length()`, and full `Mat4` multiplication to `Math.h`. Remove the `Math.h` include from `Memory.h`.

This is a four-file change that must happen before BSP types are added, otherwise `Math.h` will become unmanageable.

### Rec 3 — Redesign the render threading model (required before Phase 2)

Replace per-triangle thread dispatch with a frame-level tile job system:

```
Frame start:
  for each screen tile T:
    tile_bucket[T] = empty list

  for each visible triangle:
    determine which tiles it overlaps (via AABB)
    push triangle reference to tile_bucket[T] for each T

  for each tile T (dispatched as one job):
    for each triangle in tile_bucket[T]:
      rasterize triangle, clipped to T
```

This change eliminates all per-triangle synchronization. The only synchronization is the `wait_all()` at the end of the frame. Each tile worker is fully independent — no shared mutable state. This is also the correct architecture for a BSP renderer, where per-tile triangle lists can be filled during BSP traversal.

### Rec 4 — Replace std::function in ThreadPool (required before Phase 2)

Use a template-based job interface or a fixed-size in-place callable:

```cpp
// Option A: template enqueue (zero allocation for any callable)
template<typename F>
void enqueue(F&& f) {
    // Store F in a fixed-size buffer or a type-erased wrapper
}
```

At minimum, use `std::move_only_function<void()>` (C++23) if the toolchain supports it. If not, a manual fixed-size callable container sized to 64 bytes avoids all heap allocations for typical lambdas.

### Rec 5 — Make resolution runtime-configurable (required before Phase 2)

Remove `constexpr` from `WIN_WIDTH` and `WIN_HEIGHT`. Pass desired dimensions to `App::App(Arena&, int width, int height)`. Allocate the color and depth buffers with the supplied dimensions. Expose `width()` and `height()` accessors. Update all pixel-index calculations to use `m_app.width()`. The `screen_aabb` in examples becomes `{{0, 0}, {m_app.width() - 1, m_app.height() - 1}}`.

### Rec 6 — Design a ResourceManager with handle-based ownership (required before Phase 1 completion)

```cpp
// Handle — a typed index into a resource pool
template<typename T>
struct Handle { uint32_t id; bool is_valid() const { return id != 0; } };

using TextureHandle = Handle<Texture>;
using ModelHandle   = Handle<Model>;

class ResourceManager {
    TextureHandle load_texture(const char* path);
    ModelHandle   load_model(const char* path);
    const Texture& get(TextureHandle h) const;
    const Model&   get(ModelHandle h) const;
    // Internally: std::unordered_map<path, handle> for dedup
};
```

Textures and models are stored in flat arrays owned by the `ResourceManager`. Assets are deduped by path. Handles are stable and copyable. This replaces direct `Texture` and `Model` construction in game code. Also add `Texture(Texture&&)` move constructor to make storage in growable containers possible.

### Rec 7 — Add near-plane clipping before the rasterizer call (required before Phase 3)

Implement Sutherland-Hodgman clipping against the near plane (`z > near_z`) before calling `draw_triangle`. A clipped triangle may produce 0, 1, or 2 output triangles. This can be done with a small fixed-size output array (no heap allocation). The arena marker pattern (Rec 1) supports this: allocate the clipped polygon list per-frame into the temporary arena region.

### Rec 8 — Add left-boundary pixel mask in the rasterizer tile x-loop (immediate correctness)

Mirror the existing right-boundary mask with a left-boundary mask:

```cpp
// Existing right-boundary mask (Rasterizer.h:138-141):
if (tx + 7 > max_x) {
    int boundary_mask = (1 << (max_x - tx + 1)) - 1;
    mask &= boundary_mask;
}

// Add symmetrically before it:
if (tx < min_x) {
    int left_mask = ~((1 << (min_x - tx)) - 1) & 0xFF;
    mask &= left_mask;
}
```

Extend the AABB Clipping test case in `tests/test_engine.cpp` to assert that no pixels are written outside the clip region, not merely that the expected count is written inside it. This is the only way the test will detect the left-boundary regression.

### Rec 9 — Fix the Texture destructor allocation mismatch (deferred but tracked)

Track which allocation path owns `data` and call the matching deallocator:

```cpp
class Texture {
    ...
    bool m_owns_stbi = false;  // true if allocated via stbi_load, false if std::malloc
};

Texture::~Texture() {
    if (data) {
        if (m_owns_stbi) stbi_image_free(data);
        else std::free(data);
    }
}
```

This is safe to defer until a custom `STBI_MALLOC` is introduced, but it should be tracked as it is a latent correctness defect.

### Rec 10 — Move the Rasterizer inner-loop y-bounds check outward (immediate, performance)

```cpp
// Current (bounds check inside inner loop):
for (int32_t y = ty; y < ty + TILE_SIZE; ++y) {
    if (y < min_y || y > max_y) { /* skip */ continue; }
    ...
}

// Fix (compute clamped range before the loop):
int32_t y_start = std::max(ty, min_y);
int32_t y_end   = std::min(ty + TILE_SIZE, max_y + 1);
for (int32_t y = y_start; y < y_end; ++y) {
    // No bounds check needed here
    ...
}
```

This removes a conditional branch from every row of every tile and simplifies the inner loop. It also corrects the unnecessary edge advancement for skipped rows.

---

## 7. Summary Table

| Area | Current State | Risk if Unchanged | Priority |
|---|---|---|---|
| Arena markers | Not implemented | Phase 1 deliverable blocked | P1 |
| Render thread model | Per-triangle dispatch | 60 FPS target unreachable at 10K tris | P1 |
| Dynamic resolution | Compile-time constant | Phase 2 blocked | P1 |
| std::function in ThreadPool | Heap allocation per triangle | Violates zero-alloc rule | P1 |
| ResourceManager / asset handles | Direct ownership, no cache | Phase 1 resource cache blocked | P1 |
| Near-plane clipping | Absent | Phase 3 corruption when player near geometry | P2 |
| Math.h / Geometry.h split | Mixed in one file | BSP phase adds types, file unmanageable | P2 |
| Rasterizer left-boundary mask | Missing | Out-of-bounds fragment shader invocation | Immediate |
| Rasterizer y-bounds check | Inside inner loop | Branch overhead in SIMD inner loop | Deferred |
| Texture destructor mismatch | stbi_image_free on malloc'd data | Latent heap corruption if stb allocator overridden | Deferred |
| Memory.h includes Math.h | Unnecessary coupling | Minor but cascading compile dependency | Deferred |

### Resolved Since Original Audit

| Fix | Was | Now |
|---|---|---|
| FIX-01: Arena overflow guard | `assert` (stripped in Release) | `fprintf` + `abort()` unconditional |
| FIX-02: `is_color_near` subtraction | Silent UB on `uint8_t` subtraction | Casts to `int` before `abs` |
| FIX-03: `ThreadPool::stop` race | `bool` — data race under TSan | `std::atomic<bool>` with release/acquire |
| FIX-04: `ThreadPool::active_tasks` synchronization | `std::atomic<uint32_t>` + mutex (mixed) | `uint32_t` under `queue_mutex` only |
| FIX-05: Bounding sphere radius | Returns AABB half-diagonal (too large) | Exact `max\|v - center\|` over all vertices |
| FIX-06: `Texture::is_valid()` and guards | Missing `width > 0 && height > 0` check | Full check; `sample`/`sample8` guarded; in-memory constructor added |
| FIX-07: Rasterizer tile-rejection parentheses | `a * (TILE_SIZE-1) << SUBPIXEL_SHIFT` | `(a * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT` |
| FIX-08: CMake GLOB_RECURSE | Stale build on new `.cpp` | Explicit source list in `engine/CMakeLists.txt` |
