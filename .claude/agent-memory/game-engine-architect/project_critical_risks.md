---
name: project-retro3d-critical-risks
description: Architectural risks that will require painful refactoring if not addressed before specific roadmap phases — documented in architecture_audit.md
metadata:
  type: project
---

Critical risks identified in the 2026-05-17 architecture audit (`doc/architecture_audit.md`). Each risk is tagged with the phase that will be blocked.

**Before Phase 2:**
- Resolution is `constexpr` in `App` — dynamic resolution (Phase 2 deliverable) requires a full refactor of App, buffer allocation, and all pixel-index calcs.
- Per-triangle thread dispatch (`ThreadPool::wait_all` called once per triangle) cannot meet the 10K-tri @ 60 FPS target. Must be replaced with frame-level tile jobs before Phase 2 adds ECS overhead.
- `std::function<void()>` in `ThreadPool::enqueue` triggers heap allocation per triangle (exceeds SBO). Violates GEMINI.md zero-alloc rule.

**Before Phase 1 completion:**
- `Arena` has no markers — temporary per-frame allocations cannot coexist with persistent ones. Phase 1 deliverable explicitly requires markers.
- `Model` and `Texture` use unmanaged heap; no handle/cache system. Resource cache (Phase 1) blocked.

**Before Phase 3:**
- No near-plane clipping before rasterizer. Safe now (model at fixed z=600), fatal once player camera can approach geometry.
- `Math.h` contains render-domain types (Triangle, AABB) — will become unmanageable when BSP adds Plane, Ray, Frustum, etc.

**Immediate (correctness):**
- `Arena::allocateImpl` uses `assert` for overflow — stripped by NDEBUG, silent OOB write in Release (Bug 2 in test_audit.md).
- `ThreadPool::stop` is non-atomic bool (data race under TSan — Bug 1 in test_audit.md).

**Why:** Captured from architecture audit so future conversations start from the known risk baseline rather than re-deriving it.
**How to apply:** When a user asks to implement any Phase 1+ feature, check this list first and flag any blockers before proceeding.
