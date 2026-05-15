#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/core/Math.h>
#include <cmath>

TEST_CASE("Vec3 Operations") {
    Vec3f a = {1.0f, 2.0f, 3.0f};
    Vec3f b = {4.0f, 5.0f, 6.0f};
    
    SUBCASE("Addition") {
        Vec3f c = a + b;
        CHECK(c.x == 5.0f);
        CHECK(c.y == 7.0f);
        CHECK(c.z == 9.0f);
    }
    
    SUBCASE("Subtraction") {
        Vec3f d = b - a;
        CHECK(d.x == 3.0f);
        CHECK(d.y == 3.0f);
        CHECK(d.z == 3.0f);
    }
    
    SUBCASE("Dot Product") {
        float dot = Vec3f::dot(a, b);
        CHECK(dot == doctest::Approx(32.0f));
    }
    
    SUBCASE("Cross Product") {
        Vec3f cross = Vec3f::cross(a, b);
        CHECK(cross.x == -3.0f);
        CHECK(cross.y == 6.0f);
        CHECK(cross.z == -3.0f);
    }
}

TEST_CASE("Mat4 Operations") {
    SUBCASE("Identity") {
        Mat4 id = Mat4::identity();
        Vec3f v = {1.0f, 2.0f, 3.0f};
        Vec3f v2 = id.transform(v);
        CHECK(v2.x == doctest::Approx(v.x));
        CHECK(v2.y == doctest::Approx(v.y));
        CHECK(v2.z == doctest::Approx(v.z));
    }
    
    SUBCASE("Rotation Y") {
        Mat4 rot = Mat4::rotate_y(M_PI / 2.0f); // 90 degrees
        Vec3f vr = rot.transform({1.0f, 0.0f, 0.0f});
        
        // (1,0,0) becomes (0,0,1) with current rotate_y implementation
        CHECK(vr.x == doctest::Approx(0.0f));
        CHECK(vr.y == doctest::Approx(0.0f));
        CHECK(vr.z == doctest::Approx(1.0f));
    }
}
