---
name: test-framework-conventions
description: Testing framework, style conventions, and pyramid ratios for Retro3D
metadata:
  type: project
---

## Framework
- doctest v2.5.2 (fetched via FetchContent)
- Each test binary must define `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in exactly one TU.
- Test structure: `TEST_CASE("Module Operations") { SUBCASE("behaviour when condition") { ... } }`
- Floating-point: use `doctest::Approx(x)` with a sensible epsilon (default is 1e-5).
- Skipped/benchmark tests: use `* doctest::skip(true)`.

## Current test pyramid (at audit date 2026-05-17)
- Unit: MathTests (2 cases, 6 subcases), MemoryTests (1 case, 2 subcases), ThreadTests (1 case, 1 subcase), EngineTests (2 cases, 5 subcases including 1 skipped benchmark)
- Integration: NONE
- E2E: NONE
- Untested components: Input, Draw, Texture, Model, App, Engine lifecycle, Rasterizer+ThreadPool combined path

## CMake macro for tests
```cmake
macro(add_retro_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE Retro3D doctest::doctest)
    add_test(NAME ${NAME} COMMAND ${NAME})
endmacro()
```
New test files must be added to root CMakeLists.txt using this macro.

## ASan convention
Project DoD requires ASan-clean. Enable with `-DENABLE_ASAN=ON`. Cannot combine with TSan.
TSan enabled with `-DENABLE_TSAN=ON`.

**Why:** Ensures all future test code matches existing conventions.
**How to apply:** Follow these patterns when writing new test files.
