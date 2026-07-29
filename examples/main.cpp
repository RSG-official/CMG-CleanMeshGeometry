#include "cmg_format.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file.cmg>\n";
        return 1;
    }

    CmgMesh mesh;
    if (!mesh.load(argv[1])) {
        std::cout << "Failed to load: " << argv[1] << "\n";
        return 1;
    }

    std::cout << "Loaded: " << argv[1] << "\n";
    std::cout << "  vertices: " << mesh.vertices.size() << "\n";
    std::cout << "  triangles: " << (mesh.indices.size() / 3) << "\n";
    std::cout << "  bbox: min(" << mesh.bbox.min.x << "," << mesh.bbox.min.y << "," << mesh.bbox.min.z
               << ") max(" << mesh.bbox.max.x << "," << mesh.bbox.max.y << "," << mesh.bbox.max.z << ")\n";
    std::cout << "  sidecar loaded: " << (mesh.hasSidecar ? "yes" : "no") << "\n";
    if (mesh.hasSidecar) {
        std::cout << "  objects: " << mesh.objects.size() << "\n";
        std::cout << "  materials: " << mesh.materials.size() << "\n";
        std::cout << "  UVs in pool: " << (mesh.uvPool.size() / 2) << "\n";
        std::cout << "  textures: " << mesh.textures.size() << "\n";
        for (const auto& t : mesh.textures) {
            std::cout << "    texture \"" << t.name << "\" role=" << (int)t.role
                       << " mode=" << (t.mode == TEXTURE_MODE_EMBEDDED ? "embedded" : "external") << "\n";
        }
    }
    return 0;
}
