---
name: bugs-and-patterns
description: Bugs, suspicious code, and recurring patterns found during code analysis
metadata:
  type: project
---

## Confirmed bugs / high-risk issues found during audit (2026-05-17)

1. **ThreadPool::stop is not atomic** (`ThreadPool.h` line 30). `stop` is a plain `bool`, written from the destructor (under lock) and read from worker lambdas (under lock), but the `condition.notify_all()` call after `stop = true` happens outside the lock in the destructor. This is a data race under TSAN.

2. **Arena overflow is assert-only** (`Memory.h` line 69). `allocateImpl` uses `assert()` to guard the overflow check. In Release builds (`-DNDEBUG`) this silently UBs — writing past the backing buffer. No exception or error-return path exists.

3. **ThreadPool::wait_all race** (`ThreadPool.cpp` lines 48-53). The `wait_all` predicate checks `tasks.empty() && active_tasks == 0`. There is a TOCTOU window: a task can dequeue itself (tasks.empty() == true) but not yet decrement `active_tasks`, so a caller could see `tasks.empty() && active_tasks == 0` momentarily true before the task finishes executing. The correct fix is to decrement `active_tasks` *before* notifying `wait_condition`, or use a separate "in-flight" counter incremented at enqueue and decremented after task body.

4. **Model::radius is max extent, not radius** (`Model.cpp` line 104). `radius = std::max({size.x, size.y, size.z})` — this is the largest dimension, not the bounding-sphere radius. The actual bounding sphere radius should be `std::sqrt(size.x^2 + size.y^2 + size.z^2) / 2` or at minimum `max_dim / 2`. The field is named `get_bounding_sphere_radius()` so callers (chest_viewer line 42) divide by it to compute scale — causing a factor-of-2 scale error.

5. **Rasterizer tile-rejection operator precedence** (`Rasterizer.h` line 111). `a * (TILE_SIZE-1) << SUBPIXEL_SHIFT` — `<<` has lower precedence than `*`, so this evaluates as `(a * (TILE_SIZE-1)) << SUBPIXEL_SHIFT`, which is likely the intent, but the expression should be parenthesised explicitly to prevent silent breakage if SUBPIXEL_SHIFT ever changes.

6. **Texture::sample8 — no size guard when width/height == 0** (`Texture.cpp` line 64). `_mm256_set1_epi32(width - 1)` when `width == 0` produces `_mm256_set1_epi32(-1)`, then `_mm256_min_epi32(iu, -1)` clamps all indices to -1, causing a gather with negative (wrapping) offsets — UB / out-of-bounds read.

7. **Vec3f::operator* missing operator/(float)** (`Math.h`). Division is absent; callers use `v * (1.0f / s)` which introduces an extra reciprocal and is inconsistent.

8. **Mat4::transform ignores w component** (`Math.h` lines 68-74). The transform only adds `m[3][*]` (translation) but does not apply the 4th row — the transform is implicitly only affine. This is fine as long as perspective division is never expected from `transform`, but there is no documentation of the constraint.

## Recurring patterns to watch
- AVX2 intrinsics used with no scalar fallback path; non-AVX2 machines will SIGILL at runtime.
- `std::cout`/`std::cerr` inside library code (Texture, Model constructors) — hard to suppress in tests.
- No error propagation from constructors (Texture loads in constructor, stores nullptr on failure, caller checks `is_valid()`).

**Why:** Recorded during first full audit.
**How to apply:** Write targeted regression tests for each of these; flag them in PR reviews.
