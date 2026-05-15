#ifndef INC_3D_TEST_MODEL_H
#define INC_3D_TEST_MODEL_H

#include <retro3d/core/Math.h>
#include <vector>
#include <string>

struct Vertex {
    Vec3f position;
    Vec2f texcoord;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class Model {
public:
    Model() = default;
    bool load(const char* path);

    const std::vector<Mesh>& get_meshes() const { return meshes; }
    Vec3f get_center() const { return center; }
    float get_bounding_sphere_radius() const { return radius; }

private:
    std::vector<Mesh> meshes;
    Vec3f center{0, 0, 0};
    float radius = 0.0f;
};

#endif //INC_3D_TEST_MODEL_H
