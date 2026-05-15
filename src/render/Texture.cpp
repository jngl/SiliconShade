#include <retro3d/render/Texture.h>
#include <stb_image.h>
#include <iostream>
#include <algorithm>
#include <cmath>

Texture::Texture(const char* path) {
    data = stbi_load(path, &width, &height, &channels, 4);
    if (data) {
        std::cout << "Texture chargée : " << path << " (" << width << "x" << height << ")" << std::endl;
        
        // Swizzle RGBA to ARGB to match SDL_PIXELFORMAT_ARGB8888
        // RGBA: [R, G, B, A] -> ARGB: [A, R, G, B]
        // On little-endian, uint32_t 0xAARRGGBB corresponds to bytes [BB, GG, RR, AA]
        // stb_load with 4 channels returns bytes [RR, GG, BB, AA]
        uint32_t* p = reinterpret_cast<uint32_t*>(data);
        for (int i = 0; i < width * height; ++i) {
            uint8_t r = (p[i] >> 0) & 0xFF;
            uint8_t g = (p[i] >> 8) & 0xFF;
            uint8_t b = (p[i] >> 16) & 0xFF;
            uint8_t a = (p[i] >> 24) & 0xFF;
            p[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    } else {
        std::cerr << "ERREUR : Impossible de charger " << path << std::endl;
    }
}

Texture::~Texture() {
    if (data) {
        stbi_image_free(data);
    }
}

uint32_t Texture::sample(float u, float v) const {
    if (!data) return 0xFFFFFFFF;

    // Wrap-around
    float u_wrapped = u - std::floor(u);
    float v_wrapped = v - std::floor(v);

    int iu = std::clamp((int)(u_wrapped * (float)width), 0, width - 1);
    int iv = std::clamp((int)(v_wrapped * (float)height), 0, height - 1);

    const uint32_t* p = reinterpret_cast<const uint32_t*>(data);
    return p[iv * width + iu];
}

__m256i Texture::sample8(__m256 u, __m256 v) const {
    if (!data) return _mm256_set1_epi32(0xFFFFFFFF);

    // Wrap-around: u - floor(u)
    u = _mm256_sub_ps(u, _mm256_floor_ps(u));
    v = _mm256_sub_ps(v, _mm256_floor_ps(v));

    __m256 v_width = _mm256_set1_ps((float)width);
    __m256 v_height = _mm256_set1_ps((float)height);

    // Multiply by dimensions and convert to int
    __m256i iu = _mm256_cvttps_epi32(_mm256_mul_ps(u, v_width));
    __m256i iv = _mm256_cvttps_epi32(_mm256_mul_ps(v, v_height));

    // Clamp to [0, width-1] / [0, height-1]
    iu = _mm256_max_epi32(_mm256_setzero_si256(), _mm256_min_epi32(iu, _mm256_set1_epi32(width - 1)));
    iv = _mm256_max_epi32(_mm256_setzero_si256(), _mm256_min_epi32(iv, _mm256_set1_epi32(height - 1)));

    // Calculate linear indices: iv * width + iu
    __m256i indices = _mm256_add_epi32(_mm256_mullo_epi32(iv, _mm256_set1_epi32(width)), iu);

    // Gather pixels (using 32-bit offsets)
    return _mm256_i32gather_epi32(reinterpret_cast<const int*>(data), indices, 4);
}
