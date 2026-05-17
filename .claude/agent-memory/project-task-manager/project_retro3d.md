---
name: project-retro3d-task-list
description: Retro3D engine task list generated 2026-05-17 from PLAN.md and two audit reports; covers bugs, Phases 1-6, and CI tasks
metadata:
  type: project
---

On 2026-05-17 a consolidated task list was generated and written to `doc/tasks.md`.
It covers 8 immediate bug fixes, Phase 1–6 roadmap tasks (merged with architecture audit
findings), and 3 CI/build tasks.

**Why:** The developer needed a single actionable file that merges the 6-phase PLAN.md roadmap
with findings from `doc/test_audit.md` (7 bugs, ~18% coverage) and `doc/architecture_audit.md`
(6 architectural risks).

**How to apply:** When the developer asks about next steps or priorities, reference `doc/tasks.md`
as the canonical task source. The ordering within each section is intentional — prerequisites are
listed per task.

Key prerequisite chains to remember:
- FIX-01 (Arena overflow) must precede P1-03 (Arena markers)
- FIX-03 + FIX-04 (ThreadPool races) must precede P1-05 (replace std::function)
- P1-01 (Math.h complete) must precede P1-02 (Geometry.h split)
- P1-03 (Arena markers) must precede P2-03 (dynamic resolution) and P2-01 (tile dispatch)
- P2-01 (tile dispatch) is required before Phase 2 ECS work to hit the 60 FPS target

See [[project-retro3d-audit-summary]] for a summary of the audit findings.
