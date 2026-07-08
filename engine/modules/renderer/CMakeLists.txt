# renderer — Layer 3 (gameplay). A high-level scene renderer over the `render` RHI:
# a MeshRenderer component makes an ECS entity draw itself, and renderMeshes() draws
# every such entity (with scene::WorldTransform) into the open render pass, pushing
# each world matrix as a push constant. Bridges render + scene + ecs so games stop
# hand-wiring mesh->draw. Namespace zuki::renderer.
add_zuki_module(renderer PUBLIC_DEPS core math render scene ecs)
