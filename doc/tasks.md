# Retro3D — Consolidated Task List

**Generated:** 2026-05-17  
**Sources:** `PLAN.md`, `GEMINI.md`, `doc/test_audit.md`, `doc/architecture_audit.md`

Each task carries a prerequisite chain. Work top-to-bottom within each section.
Tick a box when the task satisfies the Definition of Done (unit tests pass under ASan,
PLAN.md updated, no performance regression).

---

## Section 0 — Immediate Fixes

These are bugs and correctness defects identified by the audits. They must be resolved
before any new feature work begins. None require architectural decisions.

---

- [ ] **FIX-01: Harden Arena overflow guard for Release builds**

  **File:** `engine/include/retro3d/core/Memory.h:69`  
  **Source:** `doc/test_audit.md` Bug 2 / `doc/architecture_audit.md` Rec 8

  - Replace `assert(current + padding + size <= memory_bloc.size)` with an unconditional
    `std::fprintf` + `std::abort()` block that survives `-DNDEBUG`.
  - The replacement guard must print the requested size and remaining capacity before aborting.
  - Add a test in `tests/test_memory.cpp` that triggers overflow and verifies the abort path
    (use `REQUIRE_THROWS` or a death-test wrapper).

  **Why it matters:** In Release builds the assert is stripped. Any over-allocation silently
  writes past the backing buffer — undefined behavior that corrupts the heap with no diagnostic.  
  **Prerequisites:** None.

---

- [ ] **FIX-02: Fix `is_color_near` signed/unsigned subtraction in test helper**

  **File:** `tests/test_engine.cpp:16`  
  **Source:** `doc/test_audit.md` Bug 7

  - Cast both `uint8_t` operands to `int` before subtraction:
    `std::abs((int)br - (int)r) <= tolerance`.
  - Verify that the Depth Buffer and AABB Clipping test cases now catch previously silently
    passing wrong values by running the tests before and after the fix.

  **Why it matters:** `std::abs` on an unsigned value is a no-op; comparisons in existing test
  cases silently pass incorrect pixel values, masking real rasterizer bugs.  
  **Prerequisites:** None.

---

- [ ] **FIX-03: Make `ThreadPool::stop` atomic**

  **File:** `engine/src/core/ThreadPool.cpp`, `engine/include/retro3d/core/ThreadPool.h:30`  
  **Source:** `doc/test_audit.md` Bug 1

  - Change `bool stop = false` to `std::atomic<bool> stop{false}`.
  - Use `memory_order_release` on the write and `memory_order_acquire` on the read inside
    the condition-variable predicate.
  - Run the thread tests under TSan (`-fsanitize=thread`) to confirm the race is gone.

  **Why it matters:** The unsynchronized write/read of `stop` is a real data race. TSan flags it;
  the thread pool destructor is undefined behaviour in the current implementation.  
  **Prerequisites:** None.

---

- [ ] **FIX-04: Fix `ThreadPool::wait_all` mixed synchronization invariant**

  **File:** `engine/src/core/ThreadPool.cpp:16-22, 48-53`  
  **Source:** `doc/test_audit.md` Bug 3

  - Ensure `active_tasks` is decremented while the mutex is held, so the completion invariant
    is protected by exactly one mechanism (the mutex + condition variable).
  - Remove any redundant double-check that reads the atomic outside the lock for the same
    invariant.
  - Add a test case: enqueue N tasks that sleep briefly, call `wait_all`, then assert that
    all N results have been written.

  **Why it matters:** Mixing an atomic counter and a mutex for the same invariant is fragile
  and will produce hard-to-reproduce races when the captured state grows with ECS data.  
  **Prerequisites:** FIX-03 (atomic `stop` must be correct first).

---

- [ ] **FIX-05: Fix `Model::get_bounding_sphere_radius` — returns a diameter, not a radius**

  **File:** `engine/src/render/Model.cpp:103-104`  
  **Source:** `doc/test_audit.md` Bug 4

  - Replace the current formula with: `max over all vertices of |v - center|`, or conservatively
    `std::sqrt(size.x*size.x + size.y*size.y + size.z*size.z) / 2.0f`.
  - Add a unit test with a known AABB (e.g., unit cube centered at origin) and assert the
    radius equals `sqrt(3)/2 ≈ 0.866`.

  **Why it matters:** The chest viewer uses this value directly for scaling, producing a model
  rendered at roughly half the correct size.  
  **Prerequisites:** None.

---

- [ ] **FIX-06: Guard `Texture::sample8` against zero-dimension textures**

  **File:** `engine/src/render/Texture.cpp:64`  
  **Source:** `doc/test_audit.md` Bug 5

  - Add `width > 0 && height > 0` to `Texture::is_valid()`.
  - Add a test: construct a `Texture` with width = 0, call `is_valid()`, assert it returns false,
    and assert that `sample8` is not reachable on an invalid texture.

  **Why it matters:** `width - 1` wraps to `INT_MAX` when `width == 0`, passing an out-of-bounds
  index to the AVX2 gather and causing undefined behaviour.  
  **Prerequisites:** None.

---

- [ ] **FIX-07: Add explicit parentheses to Rasterizer tile-rejection expression**

  **File:** `engine/include/retro3d/render/Rasterizer.h:111`  
  **Source:** `doc/test_audit.md` Bug 6

  - Rewrite `a * (TILE_SIZE-1) << SUBPIXEL_SHIFT` as `(a * (TILE_SIZE - 1)) << SUBPIXEL_SHIFT`.
  - No behaviour change — this is a readability and refactoring-safety fix only.

  **Why it matters:** The lack of parentheses makes the expression fragile under future edits;
  any reordering could silently change operator precedence.  
  **Prerequisites:** None.

---

- [ ] **FIX-08: Replace `GLOB_RECURSE` with explicit source lists in CMakeLists.txt**

  **File:** `engine/CMakeLists.txt`  
  **Source:** `doc/architecture_audit.md` Risk 7 / Rec 9

  - Replace the `file(GLOB_RECURSE ENGINE_SOURCES ...)` call with an explicit
    `add_library(Retro3D STATIC ...)` listing each `.cpp` file by name
    (see the exact list in `doc/architecture_audit.md` Rec 9).
  - Verify a clean build after the change.

  **Why it matters:** `GLOB_RECURSE` does not re-run CMake when new source files are added,
  causing silent missing-file build failures.  
  **Prerequisites:** None.

---

## Section 1 — Phase 1: Foundations & Infrastructure

Phase 1 goal: establish solid foundations for 3D transforms and resource management.
Architecture audit findings are merged into the relevant roadmap items below.

---

- [ ] **P1-01: Extend Math.h — complete `Vec3f`, `Mat4`, and add `Quat`**

  **Files:** `engine/include/retro3d/core/Math.h`  
  **Source:** `PLAN.md` Phase 1 §1 / `doc/architecture_audit.md` §3.1

  - Add `Vec3f::normalize()`, `Vec3f::length()`, and `Vec3f::dot()` if absent.
  - Add full 4×4 matrix multiply (`Mat4::operator*`) — the current `transform()` drops the W
    component and is not a full 4D transform.
  - Add `Mat4::perspective(fov, aspect, near, far)` and `Mat4::look_at(eye, center, up)`.
  - Add `Quat` type with `from_axis_angle`, `to_mat4`, and `slerp`.
  - Maintain a scalar reference implementation alongside any SIMD path; add `doctest::Approx`
    to all float equality checks in `tests/test_math.cpp`.

  **Why it matters:** The current `Mat4` cannot form a full MVP pipeline (model × view × projection)
  which is required for Phase 2 Camera and ECS rendering.  
  **Prerequisites:** FIX-07 (Rasterizer.h must be stable before Math.h is widely included).

---

- [ ] **P1-02: Introduce `Geometry.h` — split render-domain types out of `Math.h`**

  **Files:** `engine/include/retro3d/core/Math.h` (edit),
  new `engine/include/retro3d/core/Geometry.h`  
  **Source:** `doc/architecture_audit.md` §1.4 / Rec 2

  - Move `Triangle`, `AABB`, `aabbOfTriangle`, `aabbIntersect` from `Math.h` to `Geometry.h`.
  - Add Phase 1 spatial primitives to `Geometry.h`: `Plane`, `Ray`, `Sphere`.
  - Update all `#include` sites across the engine and examples.
  - Remove the `#include "Math.h"` line from `Memory.h` (it is unused — see `doc/architecture_audit.md` §1.3).

  **Why it matters:** Without this split, the BSP phase will pile `Frustum`, clip planes, and PVS
  types into `Math.h`, turning it into an unmanageable monolith.  
  **Prerequisites:** P1-01 (Math.h must be complete before being split).

---

- [ ] **P1-03: Add Arena markers and sub-arena support**

  **Files:** `engine/include/retro3d/core/Memory.h`, `engine/src/core/Memory.cpp`  
  **Source:** `PLAN.md` Phase 1 §2 / `doc/architecture_audit.md` §3.1 / Rec 1

  - Add `ArenaMarker` struct (`uint64_t offset`) and two methods:
    `ArenaMarker Arena::get_marker() const` and `void Arena::restore_to_marker(ArenaMarker)`.
  - Add `Arena Arena::sub_arena(size_t size)` that carves a sub-region from the current arena
    (used for thread-local arenas in P1-04 and for per-frame temporary allocations).
  - Refactor `Arena::clear()` to call `restore_to_marker({0})` for consistency.
  - Add `View<T>::operator[](size_t i)` with a debug-mode `assert(i < size)`.
  - Add tests: push marker, allocate temp data, restore marker, re-allocate in same region,
    verify the old data is overwritten and the new pointer is at the expected offset.

  **Why it matters:** Without markers, temporary per-frame allocations (clipped polygon lists,
  BSP traversal scratch space) cannot coexist with persistent data in the same arena. This is a
  named Phase 1 deliverable.  
  **Prerequisites:** FIX-01 (overflow guard must be unconditional before exercising the allocator).

---

- [ ] **P1-04: Add Pool Allocator and Thread-Local Arenas**

  **Files:** `engine/include/retro3d/core/Memory.h`, `engine/src/core/Memory.cpp`  
  **Source:** `PLAN.md` Phase 1 §2

  - Implement a `PoolAllocator<T>` that allocates from a fixed-size arena-backed block,
    with `T* acquire()` and `void release(T*)`. Use a freelist stored in the pool block itself.
  - Implement thread-local arena support: a wrapper or factory that creates one `Arena`
    sub-region per thread (using `Arena::sub_arena` from P1-03), accessible via thread-local
    storage.
  - Add unit tests: allocate and release 1000 objects from the pool, verify no heap allocation
    occurs (instrument with a custom allocator or check arena cursor movement).

  **Why it matters:** ECS component storage (Phase 2) requires a pool allocator to avoid
  fragmentation; parallel rendering (Phase 2+) requires per-thread scratch arenas to satisfy
  the zero-allocation-in-hot-path rule from `GEMINI.md`.  
  **Prerequisites:** P1-03 (sub-arena must exist before thread-local arenas are created).

---

- [ ] **P1-05: Replace `std::function` in ThreadPool with a fixed-size callable**

  **Files:** `engine/include/retro3d/core/ThreadPool.h`, `engine/src/core/ThreadPool.cpp`  
  **Source:** `doc/architecture_audit.md` §2.2 Issue 2 / Risk 6 / Rec 4

  - Replace `std::function<void()>` in the job queue with either:
    (a) `std::move_only_function<void()>` (C++23, preferred if toolchain supports it), or
    (b) a manual 64-byte fixed-size in-place callable that stores the closure without heap allocation.
  - Add a test that enqueues a lambda capturing ~60 bytes of data and verifies, via arena cursor
    inspection, that no heap allocation occurs.
  - Re-run the thread tests under ASan to confirm no heap allocations leak.

  **Why it matters:** Every `draw_triangle` call with multithreading currently causes a heap
  allocation inside the render loop, directly violating the `GEMINI.md` zero-alloc rule. The
  violation will worsen as ECS system state is captured.  
  **Prerequisites:** FIX-03, FIX-04 (ThreadPool synchronization must be correct first).

---

- [ ] **P1-06: Design and implement ResourceManager with handle-based ownership**

  **Files:** new `engine/include/retro3d/core/ResourceManager.h`,
  new `engine/src/core/ResourceManager.cpp`,
  `engine/include/retro3d/render/Texture.h`,
  `engine/include/retro3d/render/Model.h`  
  **Source:** `PLAN.md` Phase 1 §3 / `doc/architecture_audit.md` §3.1 / Risk 2 / Rec 6

  - Define `Handle<T>` (typed `uint32_t` index, `is_valid()` check).
  - Implement `ResourceManager` with `load_texture(path)`, `load_model(path)`, `get(handle)`,
    and internal path-keyed deduplication (`std::unordered_map<std::string, Handle<T>>`).
  - Add `Texture::Texture(unsigned char* data, int w, int h)` in-memory constructor
    (required for BSP embedded textures and for test isolation — see `doc/test_audit.md` Priority 4).
  - Add `Texture(Texture&&)` move constructor.
  - Assets are stored in flat arrays owned by `ResourceManager`; no asset is constructed outside it.
  - Add tests: load the same path twice, assert only one copy exists; verify `get()` on an invalid
    handle does not crash (returns a sentinel or asserts).

  **Why it matters:** Without a resource cache, the same texture is loaded and stored multiple times.
  Without stable handles, the planned ECS `MeshRenderer` component cannot safely reference assets
  across frames.  
  **Prerequisites:** P1-03 (arena-backed storage for the asset pools), P1-02 (clean include graph
  before adding a new module).

---

- [ ] **P1-07: Implement `Camera` and `Transform` classes**

  **Files:** new `engine/include/retro3d/core/Camera.h`,
  new `engine/include/retro3d/core/Transform.h`  
  **Source:** `PLAN.md` Phase 1 §4

  - `Camera`: stores position, direction, up vector; exposes `view_matrix()` (calls `Mat4::look_at`)
    and `projection_matrix(fov, aspect, near, far)` (calls `Mat4::perspective`).
  - `Transform`: stores position (`Vec3f`), rotation (`Quat`), scale (`Vec3f`); exposes
    `local_to_world()` returning a `Mat4`; supports a single-level parent pointer for simple
    hierarchy.
  - Add unit tests: identity transform produces identity matrix; a camera at (0,0,-5) looking at
    the origin produces the expected view matrix (verify against a known reference).

  **Why it matters:** A correct `Camera` with perspective and look-at is the prerequisite for
  the Phase 2 ECS render system and for frustum clipping.  
  **Prerequisites:** P1-01 (`Mat4::look_at`, `Mat4::perspective`, `Quat` must exist).

---

- [ ] **P1-08: Extend test coverage for Math, Memory, and Thread modules**

  **Files:** `tests/test_math.cpp`, `tests/test_memory.cpp`, `tests/test_thread.cpp`  
  **Source:** `doc/test_audit.md` Priority 2

  - `test_math.cpp`: add `aabbOfTriangle` (degenerate triangle, negative coords),
    `aabbIntersect` (non-overlapping), `Mat4` translation row, `Vec3f::cross` with parallel
    vectors, `rotate_y` at 0°/180°/360°, `Vec3f::operator*` scalar.
  - `test_memory.cpp`: add alignment check (`alignof(double)` and `alignof(__m256)` assertions
    on returned pointers), zeroing-after-clear test, `SystemMemory` 32-byte alignment, and the
    overflow abort test (from FIX-01).
  - `tests/test_thread.cpp`: add `ThreadPool(1)` and `ThreadPool(0)` cases, `wait_all` on
    empty queue, destructor with in-flight tasks, and a task that verifies all N indices are
    collected after `wait_all`.

  **Why it matters:** Current coverage is ~30% for Math and Memory, ~35% for ThreadPool.
  The `GEMINI.md` definition of done requires significant unit test coverage on every task.  
  **Prerequisites:** FIX-02 (test helper must be correct before new assertions are added),
  FIX-03, FIX-04 (thread tests must run against a race-free implementation).

---

## Section 2 — Phase 2: Rendering Pipeline & ECS

Phase 2 goal: high-level rendering API and a structured game world.
Two architecture-audit findings (render threading model and dynamic resolution) are required
before the ECS and UI work can be built on top.

---

- [ ] **P2-01: Redesign render threading to frame-level tile dispatch**

  **Files:** `engine/include/retro3d/render/Rasterizer.h`,
  `engine/src/core/ThreadPool.cpp`,
  `engine/include/retro3d/core/ThreadPool.h`  
  **Source:** `PLAN.md` Phase 2 §1 / `doc/architecture_audit.md` §2.2 Issue 1 / Risk 3 / Rec 3

  - Remove the `pool->wait_all()` call that currently fires once per triangle.
  - Introduce a per-frame `TileBucket` structure: one list of triangle references per screen tile.
  - During triangle submission, determine which tiles the triangle's screen AABB overlaps and
    push a reference to each relevant bucket (no heap allocation — use arena-backed storage from P1-03).
  - Dispatch one `ThreadPool` job per tile at end-of-frame; each job iterates its bucket and
    rasterizes each triangle clipped to the tile bounds.
  - Call `wait_all()` exactly once per frame.
  - Add an integration test: render 1000 triangles with the new path and compare output
    byte-for-byte against the single-threaded path.

  **Why it matters:** Per-triangle `wait_all` produces ~10,000 condition-variable waits per frame
  at 10K triangles — consuming 60% of the 16.7 ms frame budget before a pixel is drawn. The 60 FPS
  target at 10K–20K triangles is unreachable without this redesign.  
  **Prerequisites:** P1-04 (thread-local arenas for bucket storage), P1-05 (zero-alloc job queue).

---

- [ ] **P2-02: Fix rasterizer tile inner-loop y-bounds check**

  **File:** `engine/include/retro3d/render/Rasterizer.h:124-130`  
  **Source:** `doc/architecture_audit.md` §2.2 Issue 3 / Rec 10

  - Replace the per-row `if (y < min_y || y > max_y) continue;` with a pre-computed
    clamped range: `y_start = std::max(ty, min_y)` and `y_end = std::min(ty + TILE_SIZE, max_y + 1)`.
  - Verify the inner loop no longer advances edge-function state for skipped rows.
  - Run existing rasterizer tests to confirm pixel-identical output.

  **Why it matters:** The branch fires on up to 7 of 8 rows for partially-covered tiles —
  an essentially unconditional branch inside the SIMD inner loop. Removing it also fixes
  incorrect edge-function advancement for skipped rows.  
  **Prerequisites:** P2-01 (tile dispatch redesign may alter the tile loop structure).

---

- [ ] **P2-03: Make resolution runtime-configurable**

  **Files:** `engine/include/retro3d/core/App.h`, `engine/src/core/App.cpp`,
  `chest_viewer/main.cpp`, `crypt_crawler/main.cpp`  
  **Source:** `PLAN.md` Phase 2 §1 / `doc/architecture_audit.md` Risk 1 / Rec 5

  - Remove `static constexpr int32_t WIN_WIDTH` and `WIN_HEIGHT` from `App`.
  - Add `int32_t width, height` runtime fields; update the constructor signature to
    `App(Arena&, int width, int height)`.
  - Reallocate color and depth buffers using the supplied dimensions (uses arena markers from P1-03
    to support future resize without freeing persistent data).
  - Expose `int App::width() const` and `int App::height() const` accessors.
  - Update all pixel-index calculations and `send_framebuffer` row-stride to use runtime fields.
  - Update `screen_aabb` in examples to `{{0, 0}, {m_app.width()-1, m_app.height()-1}}`.

  **Why it matters:** Dynamic resolution is a named Phase 2 deliverable. Compile-time constants
  make it impossible to resize the framebuffer at runtime without a full refactor at a worse moment.  
  **Prerequisites:** P1-03 (arena markers required for resizable buffer allocation).

---

- [ ] **P2-04: Implement the ECS core — `World`, `Entity`, and component storage**

  **Files:** new `engine/include/retro3d/ecs/World.h`,
  new `engine/include/retro3d/ecs/Entity.h`,
  new `engine/src/ecs/World.cpp`  
  **Source:** `PLAN.md` Phase 2 §2

  - `Entity`: a typed `uint32_t` ID (generation + index packed).
  - `World`: creates entities, stores components in pool-allocated arrays (one pool per component type
    via `PoolAllocator` from P1-04), and provides `add_component<T>`, `get_component<T>`,
    `remove_component<T>`.
  - Provide an iteration API: `World::view<T>()` returns a range over all entities that have T.
  - Add an `Engine` extension point: a protected `register_system(...)` that the game subclass
    calls from `on_init()`, and which the base class calls in order from `on_update()`.
  - Add unit tests: create 100 entities, add `Transform` components, iterate, verify count.

  **Why it matters:** ECS is the core architectural unit for Phase 2 and all subsequent phases.
  The `MeshRenderer` system, gameplay triggers, and physics bodies (Phases 3–4) all depend on it.  
  **Prerequisites:** P1-04 (PoolAllocator), P1-07 (Transform type).

---

- [ ] **P2-05: Implement ECS automatic render system (`MeshRenderer`)**

  **Files:** new `engine/include/retro3d/ecs/systems/RenderSystem.h`,
  `engine/include/retro3d/ecs/World.h`  
  **Source:** `PLAN.md` Phase 2 §2

  - Define a `MeshRenderer` component holding a `ModelHandle` and a `TextureHandle`.
  - Implement `RenderSystem::update(World&, Rasterizer&, Camera&)` that iterates all entities
    with `Transform` + `MeshRenderer`, computes the MVP matrix, and calls `draw_triangle` for
    each triangle of the referenced model.
  - Integrate with the frame-level tile dispatch from P2-01.

  **Why it matters:** Replaces the imperative per-frame mesh-drawing code in `on_render()` with
  a data-driven system that scales to hundreds of entities.  
  **Prerequisites:** P2-01 (tile dispatch), P2-04 (World/Entity), P1-06 (ResourceManager),
  P1-07 (Camera).

---

- [ ] **P2-06: Implement 2D software UI primitives**

  **Files:** new `engine/include/retro3d/ui/Draw2D.h`,
  new `engine/src/ui/Draw2D.cpp`  
  **Source:** `PLAN.md` Phase 2 §3

  - Filled and bordered rectangles.
  - Line drawing (Bresenham or DDA).
  - Bitmap font rendering via a texture atlas (monospace, one glyph per cell).
  - 2D sprite drawing with alpha blending (porter-duff `src-over`).
  - Add tests in `tests/test_draw.cpp`: draw a filled rect, sample corner pixels, assert colour;
    draw a sprite with a known alpha, assert blended pixel value.

  **Why it matters:** The debug overlay (P2-07), HUD (Phase 5), and menus (Phase 5) all depend
  on these 2D primitives.  
  **Prerequisites:** P2-03 (runtime resolution needed to compute correct pixel offsets).

---

- [ ] **P2-07: Implement debug overlay — FPS, triangle count, memory usage**

  **Files:** new `engine/include/retro3d/ui/DebugOverlay.h`,
  `engine/src/core/Engine.cpp`  
  **Source:** `PLAN.md` Phase 2 §4

  - `DebugOverlay::render(Draw2D&, float fps, int tri_count, size_t mem_used_bytes)` draws a
    semi-transparent panel in the top-left corner using the bitmap font from P2-06.
  - Wire it into `Engine::run()` behind a `bool debug_overlay = false` flag that the game
    subclass can toggle.

  **Why it matters:** Provides the feedback loop needed to validate performance during Phases 3–6.  
  **Prerequisites:** P2-06 (Draw2D primitives).

---

## Section 3 — Phase 3: BSP Environment

Phase 3 goal: load and display complex Half-Life / Quake style levels.
Near-plane clipping (an architecture-audit finding) is required before BSP geometry
can be traversed safely.

---

- [ ] **P3-01: Implement near-plane clipping (Sutherland-Hodgman)**

  **Files:** new `engine/include/retro3d/render/Clipper.h`,
  `engine/include/retro3d/render/Rasterizer.h`  
  **Source:** `doc/architecture_audit.md` §3.2 / Risk 5 / Rec 7

  - Implement `clip_triangle_near(triangle, near_z)` returning 0, 1, or 2 output triangles
    in a fixed-size local array (no heap allocation — use a stack-allocated `std::array<Triangle, 2>`).
  - Call the clipper on every triangle before `draw_triangle` in the render loop.
  - Add tests: a triangle fully behind the near plane produces 0 triangles; a triangle
    fully in front is unchanged; a triangle straddling the plane produces 1 or 2 correct triangles.

  **Why it matters:** Triangles with any vertex at z ≤ 0 cause division-by-zero in the
  perspective divide, corrupting depth and UV data for surviving pixels. Safe for the current
  demos (model always at z = 600) but an active bug the moment a player approaches BSP geometry.  
  **Prerequisites:** P1-02 (`Plane` type in Geometry.h), P1-07 (Camera provides near-plane value).

---

- [ ] **P3-02: Implement BSP parser for Quake/Half-Life format**

  **Files:** new `engine/include/retro3d/world/BspMap.h`,
  new `engine/src/world/BspMap.cpp`  
  **Source:** `PLAN.md` Phase 3 §1

  - Parse BSP lumps: vertices, edges, faces, texinfo, and lightmap data.
  - Load embedded textures via the `ResourceManager` (P1-06) using the in-memory constructor
    from `Texture(unsigned char*, int, int)`.
  - Store lightmap atlas as a `Texture` owned by `ResourceManager`.
  - Add a `BspMap::is_valid()` check; return false (not abort) on malformed lump headers.
  - Add a test: load a known minimal hand-crafted BSP binary, assert face count and vertex
    positions.

  **Why it matters:** BSP loading is the entry point for all Phase 3 work. The lightmap atlas
  and face geometry are consumed by the visibility system (P3-03) and the BSP renderer.  
  **Prerequisites:** P1-06 (ResourceManager with in-memory Texture constructor),
  P1-02 (Geometry.h for `Plane` type used in clip nodes).

---

- [ ] **P3-03: Implement PVS (Potentially Visible Set) and frustum culling**

  **Files:** new `engine/include/retro3d/world/Visibility.h`,
  new `engine/src/world/Visibility.cpp`  
  **Source:** `PLAN.md` Phase 3 §2

  - Parse and decompress PVS bitsets from the BSP visibility lump.
  - `Visibility::get_visible_leaves(player_leaf)` returns a bitset of visible leaf indices.
  - Implement frustum culling: `Frustum` type (6 `Plane`s) built from the camera MVP matrix;
    `Frustum::intersects(AABB)` returns a tri-state (outside / inside / partial).
  - Combine PVS + frustum: only submit BSP faces whose leaf is in the PVS and whose AABB is
    not fully outside the frustum.

  **Why it matters:** Without PVS, every face in the map is submitted to the rasterizer every
  frame — infeasible at Half-Life level complexity. Frustum culling eliminates additional faces
  behind or beside the camera.  
  **Prerequisites:** P3-02 (BSP data loaded), P3-01 (near-plane clipping safe),
  P1-02 (Geometry.h Plane/Frustum types).

---

## Section 4 — Phase 4: Physics & Gameplay

Phase 4 goal: fluid movement and interactive world entities.

---

- [ ] **P4-01: Implement BSP collision — slide-move and floor detection**

  **Files:** new `engine/include/retro3d/physics/BspCollision.h`,
  new `engine/src/physics/BspCollision.cpp`  
  **Source:** `PLAN.md` Phase 4 §1

  - Implement trace-against-BSP-clip-nodes (recursive hull trace) returning a `TraceResult`
    (fraction, normal, whether it started solid).
  - Implement slide-move: given a velocity and a series of up to 4 clip planes, deflect
    the velocity and re-trace until the move is complete or 4 clips are exhausted.
  - Implement floor detection: downward trace to determine ground height and `is_grounded` flag.
  - Add tests with a hand-crafted clip-node tree for a flat floor and a single wall.

  **Why it matters:** Without BSP collision, the player passes through all geometry.
  Slide-move is the Half-Life / Quake movement model that gives smooth wall-gliding behaviour.  
  **Prerequisites:** P3-02 (BSP clip nodes loaded), P1-02 (Plane type).

---

- [ ] **P4-02: Add MD2 animation support to `Model`**

  **Files:** `engine/include/retro3d/render/Model.h`,
  `engine/src/render/Model.cpp`  
  **Source:** `PLAN.md` Phase 4 §2

  - Parse MD2 file format: header, frame array, vertex array per frame, and texture coordinates.
  - Load all keyframes into a flat arena-backed array (via ResourceManager, not `std::vector`).
  - Add `Model::interpolate_frame(int frame_a, int frame_b, float t, Mesh& out)` that writes the
    interpolated vertex positions into a pre-allocated output mesh (no heap allocation).
  - Add tests: load a known MD2, interpolate at t=0 (expect frame A), t=1 (expect frame B),
    t=0.5 (expect midpoint).

  **Why it matters:** Enemies and interactive props in a Half-Life style game use MD2 keyframe
  animation. The interpolation pass must be zero-alloc (output written into a pre-allocated mesh).  
  **Prerequisites:** P1-06 (ResourceManager for arena-backed storage), P1-03 (arena markers
  for per-frame scratch interpolation buffer).

---

- [ ] **P4-03: Implement gameplay ECS systems — triggers and doors**

  **Files:** new `engine/include/retro3d/ecs/systems/GameplaySystem.h`,
  new `engine/src/ecs/systems/GameplaySystem.cpp`  
  **Source:** `PLAN.md` Phase 4 §3

  - Define `Trigger` component (AABB volume, callback or event type) and `Door` component
    (current open fraction, target fraction, speed).
  - `GameplaySystem::update(World&, float dt)` evaluates trigger overlaps and animates doors.
  - Callbacks are stored as function pointers (not `std::function`) to avoid hot-path allocations.

  **Why it matters:** Triggers and doors are the minimal interactivity set for a navigable
  Half-Life level. Implementing them as ECS systems ensures they compose cleanly with the
  render and physics systems.  
  **Prerequisites:** P2-04 (ECS World), P4-01 (collision for trigger overlap tests).

---

## Section 5 — Phase 5: HUD & Menus

Phase 5 goal: user-facing UI and game-state management.

---

- [ ] **P5-01: Implement in-game HUD — health, ammo, inventory, crosshair**

  **Files:** new `engine/include/retro3d/ui/Hud.h`,
  new `engine/src/ui/Hud.cpp`  
  **Source:** `PLAN.md` Phase 5 §1

  - `Hud::render(Draw2D&, const PlayerState&)` draws health bar, ammo counter, inventory icons,
    and a centred crosshair using sprites and bitmap fonts from P2-06.
  - Impact markers (screen-space hit indicators) rendered as timed decals on the HUD layer.

  **Why it matters:** The HUD is the primary feedback channel for the player during gameplay.  
  **Prerequisites:** P2-06 (Draw2D), P2-04 (ECS PlayerState component).

---

- [ ] **P5-02: Implement main menu and pause screen with game-state machine**

  **Files:** new `engine/include/retro3d/ui/Menu.h`,
  `engine/src/core/Engine.cpp`  
  **Source:** `PLAN.md` Phase 5 §2

  - Define a `GameState` enum: `Start`, `Playing`, `Paused`, `GameOver`.
  - `Engine::run()` routes `on_update` and `on_render` through the current state.
  - `Menu::render(Draw2D&, GameState)` draws the appropriate overlay.
  - Keyboard navigation between menu items uses the existing `Input` module.

  **Why it matters:** Without a game-state machine, there is no way to start, pause, or end
  a game session — required for a shippable demo.  
  **Prerequisites:** P2-06 (Draw2D), P5-01 (HUD must exist before Pause screen reuses it).

---

## Section 6 — Phase 6: Audio & Polish

Phase 6 goal: spatial audio, retro aesthetics, and final performance pass.

---

- [ ] **P6-01: Integrate Miniaudio — spatial 3D sound**

  **Files:** new `engine/include/retro3d/audio/Audio.h`,
  new `engine/src/audio/Audio.cpp`  
  **Source:** `PLAN.md` Phase 6 §1

  - Integrate `miniaudio` as a single-header library under `src/external/`.
  - Implement `AudioSource` component (position, radius, volume, looping flag).
  - Implement `AudioSystem::update(World&, Camera&)` that computes panning and attenuation from
    the distance and angle between source and listener.
  - Support OGG and WAV formats.

  **Why it matters:** Spatial audio is a significant contributor to the Half-Life atmosphere.  
  **Prerequisites:** P2-04 (ECS AudioSource component), P1-07 (Camera for listener position).

---

- [ ] **P6-02: Add software dithering post-process**

  **Files:** new `engine/include/retro3d/render/PostProcess.h`,
  `engine/src/core/App.cpp`  
  **Source:** `PLAN.md` Phase 6 §2

  - Implement ordered (Bayer matrix) dithering applied to the colour buffer after the 3D scene
    is rendered and before `send_framebuffer`.
  - Apply as an optional pass controlled by a `bool dithering_enabled` flag on `Engine`.
  - The pass must operate in-place on the colour buffer with no additional allocation.

  **Why it matters:** Dithering is the named retro-aesthetic post-process in the roadmap.
  It must be zero-alloc and run after all 3D and 2D rendering.  
  **Prerequisites:** P2-03 (runtime resolution, so dithering covers the actual buffer dimensions).

---

- [ ] **P6-03: Final SIMD optimisation pass**

  **Files:** `engine/include/retro3d/render/Rasterizer.h`,
  `engine/src/render/Texture.cpp`, `engine/include/retro3d/core/Math.h`  
  **Source:** `PLAN.md` Phase 6 §3 / `GEMINI.md` §2

  - Profile the full game loop with a representative BSP level loaded.
  - For each hot function where a scalar reference path still exists alongside the AVX2 path,
    verify result parity and remove the scalar path if it is unreachable in Release.
  - Add any missing AVX2 paths identified during Phase 3–5 work (e.g., frustum-plane tests,
    MD2 vertex interpolation).
  - Confirm the 10,000–20,000 triangle at 60 FPS target from `functional_target.md` is met.

  **Why it matters:** `GEMINI.md` requires every new intensive computation to include or plan an
  AVX2 implementation. This final pass closes any gaps opened during Phases 3–5.  
  **Prerequisites:** All Phase 3–5 tasks complete; profiling data available.

---

## Appendix — Build & CI Improvements

These tasks are not phase-gated but should be done alongside Phase 1.

---

- [ ] **CI-01: Add CI pipeline with ASan and test execution**

  **File:** new `.github/workflows/ci.yml` (or equivalent CI config)  
  **Source:** `doc/test_audit.md` Priority 5

  - Build with `-DENABLE_ASAN=ON` and run all CTest targets on every pull request.
  - Fail the build if any test fails or ASan reports an error.

  **Why it matters:** `GEMINI.md` definition of done requires ASan validation. Without CI,
  this check is manually skipped and bugs regress.  
  **Prerequisites:** FIX-01 through FIX-06 (bugs must be fixed before CI can pass).

---

- [ ] **CI-02: Add CMake coverage target**

  **File:** `engine/CMakeLists.txt`, root `CMakeLists.txt`  
  **Source:** `doc/test_audit.md` Priority 5

  - Add a `coverage` CMake target using `-fprofile-arcs -ftest-coverage` and `lcov`.
  - Emit an HTML report to `build/coverage/` and a summary line to stdout.
  - Current overall coverage is ~18% line / ~14% branch; track the number over time.

  **Why it matters:** Without a coverage target, it is impossible to know whether new tests
  are actually exercising new code paths.  
  **Prerequisites:** CI-01 (CI must exist before coverage reporting is useful).

---

- [ ] **CI-03: Move benchmark from `skip(true)` to a CMake option**

  **File:** `tests/` (benchmark test file), `CMakeLists.txt`  
  **Source:** `doc/test_audit.md` Priority 5

  - Replace the hardcoded `skip(true)` with a `#ifdef ENABLE_BENCHMARKS` guard.
  - Add `-DENABLE_BENCHMARKS=ON` as a CMake option that defines the macro.
  - Add a CI job variant that runs with `-DENABLE_BENCHMARKS=ON` on a dedicated performance
    runner (or nightly, not every commit).

  **Why it matters:** A skipped benchmark provides no regression signal. An opt-in benchmark
  run in CI gives quantitative performance feedback without slowing the default build.  
  **Prerequisites:** CI-01.
