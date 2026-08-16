#include "cmg_format.h"
#include <cstring>
#include <filesystem>

// ---------------------------------------------------------------------------
// Chunk container primitives
// ---------------------------------------------------------------------------

void writeHeader(std::ofstream& file, const char magic[4], uint32_t version, uint32_t chunkCount) {
    file.write(magic, 4);
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));
}

bool readHeader(std::ifstream& file, Header& outHeader) {
    file.read(outHeader.magic, 4);
    file.read(reinterpret_cast<char*>(&outHeader.version), sizeof(outHeader.version));
    file.read(reinterpret_cast<char*>(&outHeader.chunkCount), sizeof(outHeader.chunkCount));
    return file.gcount() == sizeof(outHeader.chunkCount);
}

void writeChunk(std::ofstream& file, const char id[4], const std::vector<uint8_t>& data) {
    file.write(id, 4);
    uint32_t size = static_cast<uint32_t>(data.size());
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

bool readChunk(std::ifstream& file, Chunk& outChunk) {
    char header[8];
    file.read(header, 8);
    if (file.gcount() < 8) return false;

    outChunk.id[0] = header[0];
    outChunk.id[1] = header[1];
    outChunk.id[2] = header[2];
    outChunk.id[3] = header[3];

    uint32_t size;
    std::memcpy(&size, header + 4, 4);

    outChunk.data.resize(size);
    file.read(reinterpret_cast<char*>(outChunk.data.data()), size);
    return true;
}

bool readChunkedFile(const std::string& filename, ChunkedFile& out) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) return false;
    if (!readHeader(in, out.header)) return false;

    for (uint32_t i = 0; i < out.header.chunkCount; i++) {
        Chunk c;
        if (!readChunk(in, c)) return false;
        out.chunks.push_back(c);
    }
    return true;
}

// ---------------------------------------------------------------------------
// pack/unpack functions
// ---------------------------------------------------------------------------

std::vector<uint8_t> packVertices(const std::vector<Vertex>& verts) {
    std::vector<uint8_t> data(verts.size() * sizeof(Vertex));
    std::memcpy(data.data(), verts.data(), data.size());
    return data;
}

std::vector<Vertex> unpackVertices(const std::vector<uint8_t>& data) {
    size_t count = data.size() / sizeof(Vertex);
    std::vector<Vertex> verts(count);
    std::memcpy(verts.data(), data.data(), data.size());
    return verts;
}

std::vector<uint8_t> packIndices(const std::vector<uint32_t>& indices) {
    std::vector<uint8_t> data(indices.size() * sizeof(uint32_t));
    std::memcpy(data.data(), indices.data(), data.size());
    return data;
}

std::vector<uint32_t> unpackIndices(const std::vector<uint8_t>& data) {
    size_t count = data.size() / sizeof(uint32_t);
    std::vector<uint32_t> indices(count);
    std::memcpy(indices.data(), data.data(), data.size());
    return indices;
}

std::vector<uint8_t> packBBox(const BoundingBox& bbox) {
    std::vector<uint8_t> data(sizeof(BoundingBox));
    std::memcpy(data.data(), &bbox, sizeof(BoundingBox));
    return data;
}

BoundingBox unpackBBox(const std::vector<uint8_t>& data) {
    BoundingBox bbox;
    std::memcpy(&bbox, data.data(), sizeof(BoundingBox));
    return bbox;
}

std::string unpackString(const std::vector<uint8_t>& data, size_t offset, size_t& outNextOffset) {
    uint16_t length;
    std::memcpy(&length, data.data() + offset, 2);
    offset += 2;

    std::string s(reinterpret_cast<const char*>(data.data() + offset), length);
    outNextOffset = offset + length;
    return s;
}

std::vector<ObjectRange> unpackObjects(const std::vector<uint8_t>& data) {
    std::vector<ObjectRange> objects;
    size_t offset = 0;

    uint32_t count;
    std::memcpy(&count, data.data() + offset, 4);
    offset += 4;

    for (uint32_t i = 0; i < count; i++) {
        ObjectRange obj;
        obj.name = unpackString(data, offset, offset);
        std::memcpy(&obj.vstart, data.data() + offset, 4); offset += 4;
        std::memcpy(&obj.vcount, data.data() + offset, 4); offset += 4;
        std::memcpy(&obj.tstart, data.data() + offset, 4); offset += 4;
        std::memcpy(&obj.tcount, data.data() + offset, 4); offset += 4;
        objects.push_back(obj);
    }
    return objects;
}

std::vector<Material> unpackMaterials(const std::vector<uint8_t>& data) {
    std::vector<Material> materials;
    size_t offset = 0;

    uint32_t count;
    std::memcpy(&count, data.data() + offset, 4);
    offset += 4;

    for (uint32_t i = 0; i < count; i++) {
        Material mat;
        mat.name = unpackString(data, offset, offset);

        std::memcpy(mat.baseColor, data.data() + offset, sizeof(mat.baseColor)); offset += sizeof(mat.baseColor);
        std::memcpy(&mat.metallic, data.data() + offset, 4); offset += 4;
        std::memcpy(&mat.roughness, data.data() + offset, 4); offset += 4;
        std::memcpy(mat.emissionColor, data.data() + offset, sizeof(mat.emissionColor)); offset += sizeof(mat.emissionColor);
        std::memcpy(&mat.emissionStrength, data.data() + offset, 4); offset += 4;

        for (int t = 0; t < 5; t++) {
            TextureSlot& slot = mat.textures[t];
            std::memcpy(&slot.textureIndex, data.data() + offset, 4); offset += 4;
            std::memcpy(&slot.uvOffsetX, data.data() + offset, 4); offset += 4;
            std::memcpy(&slot.uvOffsetY, data.data() + offset, 4); offset += 4;
            std::memcpy(&slot.uvScaleX, data.data() + offset, 4); offset += 4;
            std::memcpy(&slot.uvScaleY, data.data() + offset, 4); offset += 4;
            std::memcpy(&slot.uvRotation, data.data() + offset, 4); offset += 4;
        }

        materials.push_back(mat);
    }
    return materials;
}

std::vector<Texture> unpackTextures(const std::vector<uint8_t>& data) {
    std::vector<Texture> textures;
    size_t offset = 0;

    uint32_t count;
    std::memcpy(&count, data.data() + offset, 4);
    offset += 4;

    for (uint32_t i = 0; i < count; i++) {
        Texture tex;
        tex.name = unpackString(data, offset, offset);

        std::memcpy(&tex.role, data.data() + offset, 1); offset += 1;
        std::memcpy(&tex.mode, data.data() + offset, 1); offset += 1;

        if (tex.mode == TEXTURE_MODE_EMBEDDED) {
            uint32_t byteLen;
            std::memcpy(&byteLen, data.data() + offset, 4);
            offset += 4;
            tex.embeddedBytes.assign(data.begin() + offset, data.begin() + offset + byteLen);
            offset += byteLen;
        } else {
            tex.externalPath = unpackString(data, offset, offset);
        }

        textures.push_back(tex);
    }
    return textures;
}

std::vector<uint16_t> unpackMaterialIndices(const std::vector<uint8_t>& data) {
    size_t count = data.size() / sizeof(uint16_t);
    std::vector<uint16_t> indices(count);
    std::memcpy(indices.data(), data.data(), data.size());
    return indices;
}

std::vector<float> unpackVertexColors(const std::vector<uint8_t>& data) {
    size_t count = data.size() / sizeof(float);
    std::vector<float> colors(count);
    std::memcpy(colors.data(), data.data(), data.size());
    return colors;
}

std::vector<float> unpackUvPool(const std::vector<uint8_t>& data) {
    size_t count = data.size() / sizeof(float);
    std::vector<float> uvs(count);
    std::memcpy(uvs.data(), data.data(), data.size());
    return uvs;
}

std::vector<uint32_t> unpackUvIndices(const std::vector<uint8_t>& data) {
    return unpackIndices(data);
}

void saveBytesToFile(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool validateChunkSize(const std::string& chunkId, size_t dataSize, size_t elementSize, std::string& outError) {
    if (dataSize % elementSize != 0) {
        outError = "Chunk '" + chunkId + "' size (" + std::to_string(dataSize) +
                    " bytes) is not a multiple of its element size (" + std::to_string(elementSize) + ")";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// CmgMesh
// ---------------------------------------------------------------------------

bool CmgMesh::load(const std::string& cmgPath) {
    ChunkedFile cmgFile;
    if (!readChunkedFile(cmgPath, cmgFile)) {
        lastError = "Could not open or read: " + cmgPath;
        return false;
    }

    if (std::memcmp(cmgFile.header.magic, "CMG\0", 4) != 0) {
        lastError = "Not a valid CMG file (bad magic bytes)";
        return false;
    }
    if (cmgFile.header.version != 0) {
        lastError = "Unsupported CMG version: " + std::to_string(cmgFile.header.version);
        return false;
    }

    bool hasVert = false, hasIndx = false;

    for (const auto& c : cmgFile.chunks) {
        std::string id(c.id, 4);
        std::string chunkError;

        if (id == std::string("VERT\0", 4)) {
            if (!validateChunkSize(id, c.data.size(), sizeof(Vertex), chunkError)) {
                lastError = chunkError; return false;
            }
            vertices = unpackVertices(c.data);
            hasVert = true;
        } else if (id == std::string("INDX\0", 4)) {
            if (!validateChunkSize(id, c.data.size(), sizeof(uint32_t), chunkError)) {
                lastError = chunkError; return false;
            }
            indices = unpackIndices(c.data);
            hasIndx = true;
        } else if (id == std::string("BBOX\0", 4)) {
            if (c.data.size() != sizeof(BoundingBox)) {
                lastError = "BBOX chunk has wrong size (" + std::to_string(c.data.size()) +
                             " bytes, expected " + std::to_string(sizeof(BoundingBox)) + ")";
                return false;
            }
            bbox = unpackBBox(c.data);
        } else if (id == std::string("EXRF\0", 4)) {
            size_t next;
            sidecarName = unpackString(c.data, 0, next);
        }
    }

    if (!hasVert || !hasIndx) {
        lastError = "File is missing required VERT or INDX chunk";
        return false;
    }

    if (sidecarName.empty()) return true;

    std::filesystem::path cmgDir = std::filesystem::path(cmgPath).parent_path();
    std::filesystem::path sidecarPath = cmgDir / sidecarName;
    if (!std::filesystem::exists(sidecarPath)) {
        // Referenced sidecar not found under its stored name. Fall back to the
        // conventional "<cmgname>.cmgex" alongside the .cmg file before giving up,
        // since EXRF's stored name and the actual file can drift (e.g. asset
        // renames) while the sidecar is still colocated.
        std::filesystem::path fallbackPath = cmgDir / (std::filesystem::path(cmgPath).stem().string() + ".cmgex");
        if (fallbackPath != sidecarPath && std::filesystem::exists(fallbackPath)) {
            sidecarPath = fallbackPath;
        } else {
            return true; // referenced but missing, and no fallback found; not fatal
        }
    }

    ChunkedFile exFile;
    if (!readChunkedFile(sidecarPath.string(), exFile)) return true; // failed to read; not fatal

    if (std::memcmp(exFile.header.magic, "CMGX", 4) != 0) {
        lastError = "Sidecar '" + sidecarName + "' has bad magic bytes; skipped";
        return true; // sidecar problems don't fail the whole load
    }

    hasSidecar = true;
    for (const auto& c : exFile.chunks) {
        std::string id(c.id, 4);
        if (id == std::string("OBJS\0", 4)) {
            objects = unpackObjects(c.data);
        } else if (id == std::string("MTRL\0", 4)) {
            materials = unpackMaterials(c.data);
        } else if (id == std::string("MIDX\0", 4)) {
            std::string midxError;
            if (validateChunkSize(id, c.data.size(), sizeof(uint16_t), midxError)) {
                materialIndices = unpackMaterialIndices(c.data);
            } else {
                lastError = midxError;
            }
        } else if (id == std::string("VCOL\0", 4)) {
            vertexColors = unpackVertexColors(c.data);
        } else if (id == std::string("UVMP\0", 4)) {
            uvPool = unpackUvPool(c.data);
        } else if (id == std::string("UVIX\0", 4)) {
            uvIndices = unpackUvIndices(c.data);
        } else if (id == std::string("TXTR\0", 4)) {
            textures = unpackTextures(c.data);
        }
    }
    return true;
}
