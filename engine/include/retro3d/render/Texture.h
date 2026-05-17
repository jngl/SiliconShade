#ifndef INC_3D_TEST_TEXTURE_H
#define INC_3D_TEST_TEXTURE_H

#include <cstdint>
#include <immintrin.h>

class Texture {
public:
    explicit Texture(const char* path);
    Texture(const unsigned char* pixels, int w, int h);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    uint32_t sample(float u, float v) const;
    __m256i sample8(__m256 u, __m256 v) const;

    bool is_valid() const { return data != nullptr && width > 0 && height > 0; }
    int get_width() const { return width; }
    int get_height() const { return height; }

private:
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = nullptr;
};

#endif //INC_3D_TEST_TEXTURE_H
