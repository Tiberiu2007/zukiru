// MeshRenderer — the ECS component that makes an entity draw itself, plus the
// system that draws them all.
//
// Attach a MeshRenderer to any entity that also has a `scene::WorldTransform`;
// `renderMeshes` iterates them and records the draws into the open render pass,
// pushing each entity's world matrix as a push constant.
//
//   world.add<renderer::MeshRenderer>(entity, {.mesh = mesh, .pipeline = p, .bindGroup = g});
//   // per frame, inside a render pass:
//   renderer::renderMeshes(app.device(), scene.world());
#pragma once

#include <zuki/ecs/world.hpp>
#include <zuki/render/rhi.hpp>
#include <zuki/renderer/mesh.hpp>

namespace zuki::renderer {

// What to draw an entity as. The entity's world matrix (from its
// `scene::WorldTransform`) is pushed as a push constant, so `pipeline` must declare
// `layout(push_constant) uniform { mat4 model; }` with `pushConstantSize =
// sizeof(math::Mat4)`. `bindGroup` supplies the pipeline's per-frame / material
// resources (e.g. a shared camera uniform + a texture); leave it invalid for a
// pipeline that reads none.
struct MeshRenderer {
    Mesh mesh{};
    render::PipelineHandle pipeline{};
    render::BindGroupHandle bindGroup{};
};

// Draw every entity that has a `scene::WorldTransform` + `MeshRenderer`, into the
// currently open render pass. Pipeline / bind-group / vertex / index binds are
// issued only when they change from the previous draw, and each entity's world
// matrix rides in as a push constant. The caller owns the frame and pass and any
// per-frame uniform updates (e.g. uploading the camera's view-projection).
void renderMeshes(render::Device& device, ecs::World& world);

}  // namespace zuki::renderer
