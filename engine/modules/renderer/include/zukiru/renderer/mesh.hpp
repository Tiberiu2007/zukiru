// Mesh — GPU buffers for a drawable, plus helpers to upload/free them.
//
// A thin bundle over the RHI: a vertex buffer, an optional index buffer, and the
// element count. `uploadMesh` turns CPU `render::MeshData` (e.g. from
// `render::cubeMesh()`) into one.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/render/primitives.hpp>
#include <zukiru/render/rhi.hpp>

namespace zukiru::renderer {

// A drawable mesh: GPU vertex (+ optional index) buffers and how much to draw.
struct Mesh {
    render::BufferHandle vertexBuffer{};
    render::BufferHandle indexBuffer{};
    u32 count = 0;  // index count when indexed, else vertex count
    render::IndexType indexType = render::IndexType::U16;

    [[nodiscard]] bool indexed() const noexcept { return indexBuffer.valid(); }
    [[nodiscard]] bool valid() const noexcept { return vertexBuffer.valid() && count != 0; }
};

// Upload interleaved CPU geometry into GPU buffers (u16 indices). The vertex
// layout is the caller's pipeline's business — this only moves bytes.
[[nodiscard]] Mesh uploadMesh(render::Device& device, const render::MeshData& data);

// Free a mesh's GPU buffers.
void destroyMesh(render::Device& device, const Mesh& mesh);

}  // namespace zukiru::renderer
