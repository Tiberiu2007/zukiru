// Primitive mesh builders — CPU-side geometry ready to upload to the RHI.
//
// Backend-agnostic and header-only: a `MeshData` is interleaved vertices
// (position, normal, uv) plus a 16-bit index buffer. Feed the two spans to
// `Device::createBuffer` (Vertex / Index) and describe the layout with a
// `VertexLayout` whose attributes match `MeshVertex`.
//
//   const MeshData cube = render::cubeMesh();
//   auto vbo = device->createBuffer(BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
//   auto ibo = device->createBuffer(BufferKind::Index, cube.indices.data(), cube.indexBytes());
//   // pos @0 (Float32x3), normal @12 (Float32x3), uv @24 (Float32x2), stride 32
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/math/vec.hpp>

#include <vector>

namespace zuki::render {

// One interleaved vertex: position, normal, uv. Tightly packed (stride 32).
struct MeshVertex {
    f32 position[3];
    f32 normal[3];
    f32 uv[2];
};

// CPU geometry: interleaved vertices + a 16-bit index buffer.
struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<u16> indices;

    [[nodiscard]] usize vertexBytes() const noexcept {
        return vertices.size() * sizeof(MeshVertex);
    }
    [[nodiscard]] usize indexBytes() const noexcept { return indices.size() * sizeof(u16); }
};

// A unit cube centred on the origin with edge length `size`. 24 vertices (each
// face gets its own 4 so normals/uvs are flat) and 36 indices; each face's uv
// spans the full [0,1] square. Winding is counter-clockwise viewed from outside.
[[nodiscard]] inline MeshData cubeMesh(f32 size = 1.0f) {
    const f32 h = size * 0.5f;
    MeshData mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    // corners a,b,c,d counter-clockwise as seen from outside (facing -normal).
    const auto addFace = [&](math::Vec3 n, math::Vec3 a, math::Vec3 b, math::Vec3 c,
                             math::Vec3 d) {
        const auto base = static_cast<u16>(mesh.vertices.size());
        const math::Vec3 corners[4] = {a, b, c, d};
        const f32 uvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (int i = 0; i < 4; ++i) {
            mesh.vertices.push_back(MeshVertex{{corners[i].x, corners[i].y, corners[i].z},
                                               {n.x, n.y, n.z},
                                               {uvs[i][0], uvs[i][1]}});
        }
        const u16 quad[6] = {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                             base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)};
        mesh.indices.insert(mesh.indices.end(), quad, quad + 6);
    };

    addFace({0.0f, 0.0f, 1.0f}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h});      // +Z front
    addFace({0.0f, 0.0f, -1.0f}, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h});  // -Z back
    addFace({1.0f, 0.0f, 0.0f}, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h});      // +X right
    addFace({-1.0f, 0.0f, 0.0f}, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h});  // -X left
    addFace({0.0f, 1.0f, 0.0f}, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h});      // +Y top
    addFace({0.0f, -1.0f, 0.0f}, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h});  // -Y bottom
    return mesh;
}

}  // namespace zuki::render
