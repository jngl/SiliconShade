#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/render/Texture.h>

#include <immintrin.h>
#include <cstdint>
#include <cstring>

TEST_CASE("Texture::is_valid") {
    SUBCASE("null data from failed file load") {
        Texture t("/nonexistent/path.png");
        CHECK_FALSE(t.is_valid());
    }

    SUBCASE("zero width — data pointer alone is not enough") {
        // Construct with valid pixel data but zero dimensions.
        // is_valid() must return false even though pixels != nullptr.
        unsigned char pixel[4] = {255, 0, 0, 255};
        Texture t(pixel, 0, 4);
        CHECK_FALSE(t.is_valid());
    }

    SUBCASE("zero height") {
        unsigned char pixel[4] = {255, 0, 0, 255};
        Texture t(pixel, 4, 0);
        CHECK_FALSE(t.is_valid());
    }

    SUBCASE("valid 1x1 texture") {
        unsigned char pixel[4] = {255, 0, 0, 255};
        Texture t(pixel, 1, 1);
        CHECK(t.is_valid());
        CHECK(t.get_width() == 1);
        CHECK(t.get_height() == 1);
    }
}

TEST_CASE("Texture::sample returns sentinel on invalid texture") {
    SUBCASE("null-data texture") {
        Texture t("/nonexistent/path.png");
        CHECK(t.sample(0.0f, 0.0f) == 0xFFFFFFFF);
    }

    SUBCASE("zero-dimension texture") {
        unsigned char pixel[4] = {255, 0, 0, 255};
        Texture t(pixel, 0, 0);
        CHECK(t.sample(0.0f, 0.0f) == 0xFFFFFFFF);
    }
}

TEST_CASE("Texture::sample8 returns sentinel on invalid texture") {
    __m256 zero = _mm256_setzero_ps();

    SUBCASE("null-data texture") {
        Texture t("/nonexistent/path.png");
        __m256i result = t.sample8(zero, zero);
        alignas(32) int vals[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(vals), result);
        for (int i = 0; i < 8; ++i)
            CHECK(static_cast<uint32_t>(vals[i]) == 0xFFFFFFFFu);
    }

    SUBCASE("zero-dimension texture") {
        unsigned char pixel[4] = {255, 0, 0, 255};
        Texture t(pixel, 0, 0);
        __m256i result = t.sample8(zero, zero);
        alignas(32) int vals[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(vals), result);
        for (int i = 0; i < 8; ++i)
            CHECK(static_cast<uint32_t>(vals[i]) == 0xFFFFFFFFu);
    }
}

TEST_CASE("Texture::sample round-trips bytes from in-memory texture") {
    // sample() reads a raw uint32_t from the buffer with no swizzle.
    // On little-endian, uint32 0xAABBCCDD is stored as bytes [DD, CC, BB, AA].
    // Store two known uint32 values and verify sample() returns them unchanged.
    const uint32_t left_val  = 0xDEADBEEFu;
    const uint32_t right_val = 0x12345678u;

    alignas(4) unsigned char pixels[8];
    std::memcpy(pixels + 0, &left_val,  4);
    std::memcpy(pixels + 4, &right_val, 4);

    Texture t(pixels, 2, 1);
    REQUIRE(t.is_valid());

    CHECK(t.sample(0.0f,  0.0f) == left_val);   // u=0.0  → pixel 0
    CHECK(t.sample(0.75f, 0.0f) == right_val);  // u=0.75 → pixel 1
}
