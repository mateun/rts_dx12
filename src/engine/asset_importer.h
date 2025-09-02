#pragma once
#include <tiny_gltf.h>

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <type_traits>  // for std::is_same_v
#include <algorithm>    // for std::equal, std::clamp

#include "geometry.h"

// -------- Helper utilities --------
static inline size_t ComponentTypeByteSize(int componentType) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:           return 1;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT:          return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
        case TINYGLTF_COMPONENT_TYPE_INT:            return 4;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return 4;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          return 4;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return 8;
        default: return 0;
    }
}

static inline size_t TypeNumComponents(int type) {
    switch (type) {
        case TINYGLTF_TYPE_SCALAR: return 1;
        case TINYGLTF_TYPE_VEC2:   return 2;
        case TINYGLTF_TYPE_VEC3:   return 3;
        case TINYGLTF_TYPE_VEC4:   return 4;
        case TINYGLTF_TYPE_MAT2:   return 4;
        case TINYGLTF_TYPE_MAT3:   return 9;
        case TINYGLTF_TYPE_MAT4:   return 16;
        default: return 0;
    }
}

template <typename T>
static inline float NormalizeToFloat(T v, bool normalized) {
    if (!normalized) return static_cast<float>(v);

    if constexpr (std::is_same_v<T, int8_t>) {
        // SNORM8 → [-1,1]
        return std::clamp(static_cast<float>(v) / 127.0f, -1.0f, 1.0f);
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        // UNORM8 → [0,1]
        return static_cast<float>(v) / 255.0f;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        // SNORM16 → [-1,1]
        return std::clamp(static_cast<float>(v) / 32767.0f, -1.0f, 1.0f);
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        // UNORM16 → [0,1]
        return static_cast<float>(v) / 65535.0f;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        // SNORM32 → [-1,1] (rare in glTF, but safe)
        return std::clamp(static_cast<float>(v) / 2147483647.0f, -1.0f, 1.0f);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        // UNORM32 → [0,1] (rare)
        return static_cast<float>(v) / 4294967295.0f;
    } else {
        return static_cast<float>(v);
    }
}

static inline float ReadComponentAsFloat(const unsigned char* p, int componentType, bool normalized) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:  return *reinterpret_cast<const float*>(p);
        case TINYGLTF_COMPONENT_TYPE_DOUBLE: return static_cast<float>(*reinterpret_cast<const double*>(p));
        case TINYGLTF_COMPONENT_TYPE_BYTE:   return NormalizeToFloat(*reinterpret_cast<const int8_t*>(p), normalized);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return NormalizeToFloat(*reinterpret_cast<const uint8_t*>(p), normalized);
        case TINYGLTF_COMPONENT_TYPE_SHORT:         return NormalizeToFloat(*reinterpret_cast<const int16_t*>(p), normalized);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return NormalizeToFloat(*reinterpret_cast<const uint16_t*>(p), normalized);
        case TINYGLTF_COMPONENT_TYPE_INT:          return NormalizeToFloat(*reinterpret_cast<const int32_t*>(p), normalized);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return NormalizeToFloat(*reinterpret_cast<const uint32_t*>(p), normalized);
        default: return 0.0f;
    }
}

// Read N floats from an accessor for a given vertex index
static inline void ReadVecN(const tinygltf::Model& model,
                            const tinygltf::Accessor& accessor,
                            size_t elemIndex,
                            size_t N,
                            float* out /*size N*/) {
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf    = model.buffers[bv.buffer];

    const size_t compSize   = ComponentTypeByteSize(accessor.componentType);
    const size_t numComps   = TypeNumComponents(accessor.type);
    const size_t stride     = accessor.ByteStride(bv) ? accessor.ByteStride(bv) : compSize * numComps;
    const size_t baseOffset = bv.byteOffset + accessor.byteOffset + elemIndex * stride;

    const unsigned char* src = buf.data.data() + baseOffset;

    const bool normalized = accessor.normalized;

    for (size_t i = 0; i < N; ++i) {
        if (i < numComps) {
            out[i] = ReadComponentAsFloat(src + i * compSize, accessor.componentType, normalized);
        } else {
            out[i] = (i == 3) ? 1.0f : 0.0f;
        }
    }
}

// Read a single index value (upcast to uint32)
static inline uint32_t ReadIndex(const tinygltf::Model& model,
                                 const tinygltf::Accessor& accessor,
                                 size_t i) {
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buf    = model.buffers[bv.buffer];

    const size_t compSize   = ComponentTypeByteSize(accessor.componentType);
    const size_t stride     = accessor.ByteStride(bv) ? accessor.ByteStride(bv) : compSize;
    const size_t baseOffset = bv.byteOffset + accessor.byteOffset + i * stride;
    const unsigned char* p  = buf.data.data() + baseOffset;

    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return *reinterpret_cast<const uint8_t*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return *reinterpret_cast<const uint16_t*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return *reinterpret_cast<const uint32_t*>(p);
        default:
            throw std::runtime_error("Unsupported index component type in glTF.");
    }
}

// -------- Loader class --------
class GltfStaticMeshLoader {
public:
    // flipV: set true for DirectX-style UV (v = 1 - v)
    bool load(const std::string& path, Geometry& out, bool flipV = true) {
        out.vertices.clear();
        out.indices.clear();

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ok = false;
        if (EndsWith(path, ".glb") || EndsWith(path, ".GLB")) {
            ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        } else {
            ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }

        if (!warn.empty()) std::cerr << "[tinygltf][warn] " << warn << "\n";
        if (!ok) {
            std::cerr << "[tinygltf][error] " << err << "\n";
            return false;
        }

        size_t baseVertex = 0;

        // Iterate all meshes / primitives
        for (const auto& mesh : model.meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES &&
                    prim.mode != TINYGLTF_MODE_TRIANGLE_STRIP &&
                    prim.mode != TINYGLTF_MODE_TRIANGLE_FAN) {
                    std::cerr << "[gltf] Skipping primitive with non-triangle mode.\n";
                    continue;
                }

                // POSITION (required)
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) {
                    std::cerr << "[gltf] Primitive missing POSITION; skipping.\n";
                    continue;
                }
                const tinygltf::Accessor& posAcc = model.accessors[itPos->second];
                if (posAcc.type != TINYGLTF_TYPE_VEC3) {
                    std::cerr << "[gltf] POSITION not a VEC3; skipping.\n";
                    continue;
                }

                // UV (optional)
                const tinygltf::Accessor* uvAcc = nullptr;
                if (auto itUV = prim.attributes.find("TEXCOORD_0"); itUV != prim.attributes.end()) {
                    uvAcc = &model.accessors[itUV->second];
                    if (TypeNumComponents(uvAcc->type) < 2) uvAcc = nullptr;
                }

                // NORMAL (optional)
                const tinygltf::Accessor* normAcc = nullptr;
                if (auto itN = prim.attributes.find("NORMAL"); itN != prim.attributes.end()) {
                    normAcc = &model.accessors[itN->second];
                    if (TypeNumComponents(normAcc->type) < 3) normAcc = nullptr;
                }

                const size_t vertCount = posAcc.count;

                // Append vertex data (interleaved): pos3, uv2, norm3
                out.vertices.reserve(out.vertices.size() + vertCount * 8);

                for (size_t i = 0; i < vertCount; ++i) {
                    float p[3] = {0,0,0};
                    ReadVecN(model, posAcc, i, 3, p);
                    p[2] = -p[2];

                    float uv[2] = {0,0};
                    if (uvAcc) {
                        ReadVecN(model, *uvAcc, i, 2, uv);
                        if (flipV) uv[1] = 1.0f - uv[1];
                    }

                    float n[3] = {0,0,1};
                    if (normAcc) {
                        ReadVecN(model, *normAcc, i, 3, n);
                        n[2] = -n[2];
                    }

                    out.vertices.push_back(p[0]);
                    out.vertices.push_back(p[1]);
                    out.vertices.push_back(p[2]);

                    out.vertices.push_back(uv[0]);
                    out.vertices.push_back(uv[1]);

                    out.vertices.push_back(n[0]);
                    out.vertices.push_back(n[1]);
                    out.vertices.push_back(n[2]);
                }

                // Indices
                if (prim.indices >= 0) {
                    const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                    const size_t indexCount = idxAcc.count;

                    out.indices.reserve(out.indices.size() + indexCount);

                    if (prim.mode == TINYGLTF_MODE_TRIANGLES) {
                        size_t start = out.indices.size();
                        for (size_t i = 0; i < indexCount; ++i) {
                            const uint32_t idx = ReadIndex(model, idxAcc, i);
                            out.indices.push_back(static_cast<uint32_t>(baseVertex + idx));
                        }
                        size_t end = out.indices.size();
                        // flip triangle winding in-place
                        for (size_t i = start; i + 2 < end; i += 3) {
                            std::swap(out.indices[i + 1], out.indices[i + 2]);
                        }
                    } else if (prim.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                        for (size_t i = 2; i < indexCount; ++i) {
                            const uint32_t a = ReadIndex(model, idxAcc, i - 2);
                            const uint32_t b = ReadIndex(model, idxAcc, i - 1);
                            const uint32_t c = ReadIndex(model, idxAcc, i);
                            if ((i % 2) == 0) {
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + a));
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + b));
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + c));
                            } else {
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + b));
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + a));
                                out.indices.push_back(static_cast<uint32_t>(baseVertex + c));
                            }
                        }
                    } else if (prim.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                        const uint32_t v0 = ReadIndex(model, idxAcc, 0);
                        for (size_t i = 2; i < indexCount; ++i) {
                            const uint32_t b = ReadIndex(model, idxAcc, i - 1);
                            const uint32_t c = ReadIndex(model, idxAcc, i);
                            out.indices.push_back(static_cast<uint32_t>(baseVertex + v0));
                            out.indices.push_back(static_cast<uint32_t>(baseVertex + b));
                            out.indices.push_back(static_cast<uint32_t>(baseVertex + c));
                        }
                    }
                } else {
                    // Non-indexed
                    if (prim.mode == TINYGLTF_MODE_TRIANGLES) {
                        for (uint32_t i = 0; i < static_cast<uint32_t>(vertCount); ++i){
                            out.indices.push_back(static_cast<uint32_t>(baseVertex + i));
                        }
                        // flip triangle winding in-place
                        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
                            std::swap(out.indices[i + 1], out.indices[i + 2]);
                        }
                    } else if (prim.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                        for (uint32_t i = 2; i < static_cast<uint32_t>(vertCount); ++i) {
                            if ((i % 2) == 0) {
                                out.indices.push_back(baseVertex + i - 2);
                                out.indices.push_back(baseVertex + i - 1);
                                out.indices.push_back(baseVertex + i);
                            } else {
                                out.indices.push_back(baseVertex + i - 1);
                                out.indices.push_back(baseVertex + i - 2);
                                out.indices.push_back(baseVertex + i);
                            }
                        }
                    } else if (prim.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                        for (uint32_t i = 2; i < static_cast<uint32_t>(vertCount); ++i) {
                            out.indices.push_back(baseVertex + 0);
                            out.indices.push_back(baseVertex + i - 1);
                            out.indices.push_back(baseVertex + i);
                        }
                    }
                }

                baseVertex += vertCount;
            }
        }

        return true;
    }

private:
    static bool EndsWith(const std::string& s, const std::string& suf) {
        if (s.size() < suf.size()) return false;
        return std::equal(suf.rbegin(), suf.rend(), s.rbegin());
    }
};
