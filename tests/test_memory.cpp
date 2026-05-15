#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/core/Memory.h>
#include <retro3d/core/Basic.h>

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
