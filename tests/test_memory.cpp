#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/core/Memory.h>
#include <retro3d/core/Basic.h>

#include <csignal>
#include <csetjmp>

namespace {
    static jmp_buf g_jmpbuf;
    static volatile bool g_abort_called = false;

    void sigabrt_handler(int) {
        g_abort_called = true;
        std::longjmp(g_jmpbuf, 1);
    }
}

TEST_CASE("Arena Memory Allocator") {
    SystemMemory mem(KILOBYTES(64));
    Arena arena(mem);
    
    SUBCASE("Basic Allocation") {
        View<int> v1 = arena.allocate<int>(256); // 1024 bytes
        CHECK(v1.data != nullptr);
        CHECK(v1.size == 256);
        
        View<int> v2 = arena.allocate<int>(512); // 2048 bytes
        CHECK(v2.data != nullptr);
        // v2 should be after v1
        CHECK((uintptr_t)v2.data >= (uintptr_t)v1.data + 1024);
    }
    
    SUBCASE("Arena Clear/Reset") {
        View<int> v1 = arena.allocate<int>(256);
        arena.clear();
        View<int> v3 = arena.allocate<int>(256);
        // After clear, allocation should start at the beginning again
        CHECK(v3.data == v1.data);
    }
}

TEST_CASE("Arena overflow aborts") {
    // 1024 bytes is already a multiple of the 32-byte SystemMemory alignment
    SystemMemory mem(KILOBYTES(1));
    Arena arena(mem);

    // Fill the arena exactly to capacity
    arena.allocate<std::byte>(1024);

    // The next allocation must trigger the abort guard unconditionally,
    // even in Release builds (-DNDEBUG). Catch SIGABRT via setjmp so the
    // test process survives.
    g_abort_called = false;
    auto prev_handler = std::signal(SIGABRT, sigabrt_handler);

    if (setjmp(g_jmpbuf) == 0) {
        arena.allocate<std::byte>(1); // must overflow
        FAIL("arena overflow did not abort");
    }

    std::signal(SIGABRT, prev_handler);
    CHECK(g_abort_called);
}
