---
name: project-retro3d-audit-summary
description: Summary of key findings from the 2026-05-17 test and architecture audits for Retro3D
metadata:
  type: project
---

**Test audit** (`doc/test_audit.md`, 2026-05-17): 7 bugs found, overall coverage ~18% line / ~14%
branch. No integration or E2E tests exist. Critical gaps: multithreaded rasterizer path (zero
coverage), Input module (zero coverage), Texture sample path (zero coverage), Engine lifecycle
(zero coverage).

**Architecture audit** (`doc/architecture_audit.md`, 2026-05-17): 6 risks identified.

| Risk | Severity | Blocks |
|---|---|---|
| Arena overflow survives only in Debug (assert stripped by NDEBUG) | HIGH | everything |
| Per-triangle thread dispatch (10K barrier ops/frame at 10K tris) | HIGH | 60 FPS target |
| Hard-coded WIN_WIDTH/WIN_HEIGHT constexpr | HIGH | Phase 2 dynamic resolution |
| std::function heap alloc in ThreadPool hot path | MEDIUM | GEMINI.md zero-alloc rule |
| No Arena markers or sub-arenas | MEDIUM | Phase 1 deliverable |
| Near-plane clipping absent | MEDIUM | Phase 3 BSP safety |

**Why:** The developer requested a merged task list and will use it as the working backlog.

**How to apply:** When estimating effort for Phase 2 or 3 work, flag that P2-01 (threading
redesign) is a substantial architectural change, not a patch. Also flag that near-plane clipping
(P3-01) is a safety prerequisite for BSP work, not a nice-to-have.

See [[project-retro3d-task-list]] for the full task file location and prerequisite chains.
