// Mesh upload/teardown + the mesh-render system that draws MeshRenderer entities.
#include <zuki/renderer/mesh.hpp>
#include <zuki/renderer/mesh_renderer.hpp>

#include <zuki/math/mat.hpp>
#include <zuki/scene/components.hpp>

namespace zuki::renderer {

Mesh uploadMesh(render::Device& device, const render::MeshData& data) {
    Mesh mesh;
    if (!data.vertices.empty()) {
        mesh.vertexBuffer = device.createBuffer(render::BufferKind::Vertex, data.vertices.data(),
                                                data.vertexBytes());
    }
    if (!data.indices.empty()) {
        mesh.indexBuffer = device.createBuffer(render::BufferKind::Index, data.indices.data(),
                                               data.indexBytes());
        mesh.count = static_cast<u32>(data.indices.size());
    } else {
        mesh.count = static_cast<u32>(data.vertices.size());
    }
    mesh.indexType = render::IndexType::U16;
    return mesh;
}

void destroyMesh(render::Device& device, const Mesh& mesh) {
    if (mesh.indexBuffer.valid()) device.destroyBuffer(mesh.indexBuffer);
    if (mesh.vertexBuffer.valid()) device.destroyBuffer(mesh.vertexBuffer);
}

void renderMeshes(render::Device& device, ecs::World& world) {
    // Track what's currently bound so we only re-bind on change.
    render::PipelineHandle boundPipeline{};
    render::BindGroupHandle boundGroup{};
    render::BufferHandle boundVertex{};
    render::BufferHandle boundIndex{};

    world.each<const scene::WorldTransform, const MeshRenderer>(
        [&](const scene::WorldTransform& transform, const MeshRenderer& mr) {
            if (!mr.mesh.valid()) return;  // nothing to draw

            if (!(mr.pipeline == boundPipeline)) {
                device.bindPipeline(mr.pipeline);
                boundPipeline = mr.pipeline;
                boundGroup = {};  // new pipeline/layout — force the bind group to rebind
            }
            if (mr.bindGroup.valid() && !(mr.bindGroup == boundGroup)) {
                device.bindBindGroup(mr.bindGroup);
                boundGroup = mr.bindGroup;
            }
            if (!(mr.mesh.vertexBuffer == boundVertex)) {
                device.bindVertexBuffer(mr.mesh.vertexBuffer);
                boundVertex = mr.mesh.vertexBuffer;
            }
            if (mr.mesh.indexed() && !(mr.mesh.indexBuffer == boundIndex)) {
                device.bindIndexBuffer(mr.mesh.indexBuffer, mr.mesh.indexType);
                boundIndex = mr.mesh.indexBuffer;
            }

            const math::Mat4 model = transform.value.toMatrix();
            device.pushConstants(model.e, sizeof(model.e));
            if (mr.mesh.indexed()) {
                device.drawIndexed(mr.mesh.count);
            } else {
                device.draw(mr.mesh.count);
            }
        });
}

}  // namespace zuki::renderer
