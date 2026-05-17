#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/render/Model.h>

#include <cmath>
#include <cstdio>

// Writes a minimal OBJ to a temp file and returns the path.
// The geometry is a unit cube (vertices at ±0.5 on all axes).
// Two faces reference all 8 corners, giving:
//   center = (0, 0, 0)
//   radius = |corner - center| = sqrt(0.5² + 0.5² + 0.5²) = sqrt(3)/2 ≈ 0.866
static const char* write_unit_cube_obj() {
    static const char* path = "/tmp/retro3d_test_unit_cube.obj";
    FILE* f = std::fopen(path, "w");
    if (!f) return nullptr;
    std::fprintf(f,
        "v  0.5  0.5  0.5\n"
        "v  0.5  0.5 -0.5\n"
        "v  0.5 -0.5  0.5\n"
        "v  0.5 -0.5 -0.5\n"
        "v -0.5  0.5  0.5\n"
        "v -0.5  0.5 -0.5\n"
        "v -0.5 -0.5  0.5\n"
        "v -0.5 -0.5 -0.5\n"
        "f 1 2 3\n"
        "f 4 5 6\n"
        "f 7 8 1\n");
    std::fclose(f);
    return path;
}

TEST_CASE("Model bounding sphere radius") {
    const char* path = write_unit_cube_obj();
    REQUIRE(path != nullptr);

    Model model;
    REQUIRE(model.load(path));

    SUBCASE("center is at the AABB midpoint") {
        Vec3f c = model.get_center();
        CHECK(c.x == doctest::Approx(0.0f).epsilon(0.001f));
        CHECK(c.y == doctest::Approx(0.0f).epsilon(0.001f));
        CHECK(c.z == doctest::Approx(0.0f).epsilon(0.001f));
    }

    SUBCASE("radius equals distance from center to corner") {
        // sqrt(0.5² + 0.5² + 0.5²) = sqrt(0.75) = sqrt(3)/2
        const float expected = std::sqrt(3.0f) / 2.0f;
        CHECK(model.get_bounding_sphere_radius() == doctest::Approx(expected).epsilon(0.001f));
    }

    SUBCASE("radius is not the AABB largest dimension") {
        // The old (buggy) formula would have returned 1.0 (full side length).
        CHECK(model.get_bounding_sphere_radius() < 1.0f);
    }

    std::remove(path);
}
