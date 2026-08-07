#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

struct Vec3 {
    float x, y, z;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct Header {
    char magic[4];
    uint32_t version;
    uint32_t chunkCount;
};

struct Chunk {
    char id[4];
    std::vector<uint8_t> data;
};

struct ChunkedFile {
    Header header;
    std::vector<Chunk> chunks;
};

void writeHeader(std::ofstream& file, const char magic[4], uint32_t version, uint32_t chunkCount);
bool readHeader(std::ifstream& file, Header& outHeader);

void writeChunk(std::ofstream& file, const char id[4], const std::vector<uint8_t>& data);
bool readChunk(std::ifstream& file, Chunk& outChunk);

bool readChunkedFile(const std::string& filename, ChunkedFile& out);

struct ObjectRange {
    std::string name;
    uint32_t vstart, vcount, tstart, tcount;
};

struct TextureSlot {
    uint32_t textureIndex;
    float uvOffsetX, uvOffsetY;
    float uvScaleX, uvScaleY;
    float uvRotation;
};

struct Material {
    std::string name;
    float baseColor[4];
    float metallic;
    float roughness;
    float emissionColor[3];
    float emissionStrength;
    TextureSlot textures[5];
};

struct Texture {
    std::string name;
    uint8_t role;
    uint8_t mode;
    std::vector<uint8_t> embeddedBytes;
    std::string externalPath;
};

inline constexpr uint32_t NO_TEXTURE = 0xFFFFFFFF;
inline constexpr uint8_t TEXTURE_MODE_EMBEDDED = 0;
inline constexpr uint8_t TEXTURE_MODE_EXTERNAL = 1;

std::vector<uint8_t> packVertices(const std::vector<Vertex>& verts);
std::vector<Vertex> unpackVertices(const std::vector<uint8_t>& data);

std::vector<uint8_t> packIndices(const std::vector<uint32_t>& indices);
std::vector<uint32_t> unpackIndices(const std::vector<uint8_t>& data);

std::vector<uint8_t> packBBox(const BoundingBox& bbox);
BoundingBox unpackBBox(const std::vector<uint8_t>& data);

std::string unpackString(const std::vector<uint8_t>& data, size_t offset, size_t& outNextOffset);

std::vector<ObjectRange> unpackObjects(const std::vector<uint8_t>& data);
std::vector<Material> unpackMaterials(const std::vector<uint8_t>& data);
std::vector<uint16_t> unpackMaterialIndices(const std::vector<uint8_t>& data);
std::vector<float> unpackVertexColors(const std::vector<uint8_t>& data);
std::vector<float> unpackUvPool(const std::vector<uint8_t>& data);
std::vector<uint32_t> unpackUvIndices(const std::vector<uint8_t>& data);
std::vector<Texture> unpackTextures(const std::vector<uint8_t>& data);

void saveBytesToFile(const std::string& filename, const std::vector<uint8_t>& data);

// Returns false and fills outError if dataSize isn't a whole multiple of
// elementSize — catches corrupted/truncated chunks instead of silently
// mis-reading or truncating them.
bool validateChunkSize(const std::string& chunkId, size_t dataSize, size_t elementSize, std::string& outError);

struct CmgMesh {
    BoundingBox bbox;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string sidecarName;

    std::vector<ObjectRange> objects;
    std::vector<Material> materials;
    std::vector<uint16_t> materialIndices;
    std::vector<float> vertexColors;
    std::vector<float> uvPool;
    std::vector<uint32_t> uvIndices;
    std::vector<Texture> textures;

    bool hasSidecar = false;

    // Empty if load() succeeded with no issues. May be set even when load()
    // returns true, if the problem was non-fatal (e.g. a bad sidecar).
    std::string lastError;

    bool load(const std::string& cmgPath);
};
