#include <retro3d/render/Model.h>
#include <tiny_obj_loader.h>
#include <iostream>
#include <algorithm>
#include <unordered_map>

// Custom hash for Vertex to use with unordered_map
namespace std {
    template<> struct hash<Vec3f> {
        size_t operator()(const Vec3f& v) const {
            size_t h1 = hash<float>{}(v.x);
            size_t h2 = hash<float>{}(v.y);
            size_t h3 = hash<float>{}(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    template<> struct hash<Vec2f> {
        size_t operator()(const Vec2f& v) const {
            size_t h1 = hash<float>{}(v.x);
            size_t h2 = hash<float>{}(v.y);
            return h1 ^ (h2 << 1);
        }
    };

    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            return hash<Vec3f>{}(v.position) ^ (hash<Vec2f>{}(v.texcoord) << 1);
        }
    };
}

bool operator==(const Vec3f& a, const Vec3f& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool operator==(const Vec2f& a, const Vec2f& b) {
    return a.x == b.x && a.y == b.y;
}

bool operator==(const Vertex& a, const Vertex& b) {
    return a.position == b.position && a.texcoord == b.texcoord;
}

bool Model::load(const char* path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path)) {
        std::cerr << "ERREUR LoadObj : " << err << std::endl;
        return false;
    }

    Vec3f min_v{1e10, 1e10, 1e10}, max_v{-1e10, -1e10, -1e10};

    for (const auto& shape : shapes) {
        Mesh mesh;
        std::unordered_map<Vertex, uint32_t> unique_vertices;
        size_t index_offset = 0;
        
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            for (size_t v = 0; v < 3; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                Vertex vertex;
                
                vertex.position.x = attrib.vertices[3 * idx.vertex_index + 0];
                vertex.position.y = attrib.vertices[3 * idx.vertex_index + 1];
                vertex.position.z = attrib.vertices[3 * idx.vertex_index + 2];

                if (idx.texcoord_index >= 0) {
                    vertex.texcoord.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vertex.texcoord.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                } else {
                    vertex.texcoord = {0, 0};
                }

                auto it = unique_vertices.find(vertex);
                if (it == unique_vertices.end()) {
                    uint32_t new_index = static_cast<uint32_t>(mesh.vertices.size());
                    unique_vertices[vertex] = new_index;
                    mesh.vertices.push_back(vertex);
                    
                    min_v.x = std::min(min_v.x, vertex.position.x);
                    min_v.y = std::min(min_v.y, vertex.position.y);
                    min_v.z = std::min(min_v.z, vertex.position.z);
                    max_v.x = std::max(max_v.x, vertex.position.x);
                    max_v.y = std::max(max_v.y, vertex.position.y);
                    max_v.z = std::max(max_v.z, vertex.position.z);
                    
                    mesh.indices.push_back(new_index);
                } else {
                    mesh.indices.push_back(it->second);
                }
            }
            index_offset += 3;
        }
        meshes.push_back(std::move(mesh));
    }

    center = (min_v + max_v) * 0.5f;
    Vec3f size = max_v - min_v;
    radius = std::max({size.x, size.y, size.z});

    std::cout << "Modèle chargé : " << path << " (" << meshes.size() << " meshes)" << std::endl;
    return true;
}
