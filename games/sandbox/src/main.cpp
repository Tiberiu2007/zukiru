// Zukiru sandbox — a minimal playable demo that exercises the whole stack:
//   app (window + device + input + loop)
//   scene + ecs (a parent "formation" node with a grid of child cube nodes)
//   renderer (MeshRenderer components drawn by renderMeshes) over render
//            (ring-buffered per-frame camera uniform + per-object push constants,
//             depth-tested, textured)
//
// A grid of textured cubes spins in place while the whole formation rotates; an
// orbit camera flies around it (WASD / arrows / Q-E, Esc quits). Set the env var
// ZUKIRU_MAX_FRAMES=N to auto-quit after N frames (used for headless smoke tests).
#include <zukiru/app/app.hpp>
#include <zukiru/ecs/world.hpp>
#include <zukiru/input/input_state.hpp>
#include <zukiru/log/log.hpp>
#include <zukiru/math/math.hpp>
#include <zukiru/render/camera.hpp>
#include <zukiru/render/primitives.hpp>
#include <zukiru/render/rhi.hpp>
#include <zukiru/renderer/renderer.hpp>
#include <zukiru/scene/scene.hpp>

#include "sandbox_shaders.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>

namespace {

using namespace zukiru;

// A game-defined ECS component, layered onto scene nodes. (Drawing is handled by
// renderer::MeshRenderer, attached below.)
struct Spin {
    math::Vec3 axis{0.0f, 1.0f, 0.0f};
    f32 speed = 1.0f;
};

constexpr f32 kPi = 3.14159265358979323846f;

// An 8x8 checkerboard so faces read clearly as they turn.
[[nodiscard]] std::array<u8, 8 * 8 * 4> checkerTexture() {
    std::array<u8, 8 * 8 * 4> pixels{};
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool light = ((x + y) & 1) == 0;
            const usize i = (static_cast<usize>(y) * 8 + static_cast<usize>(x)) * 4;
            const u8 v = light ? u8{230} : u8{60};
            pixels[i + 0] = v;
            pixels[i + 1] = light ? u8{200} : u8{70};
            pixels[i + 2] = light ? u8{120} : u8{90};
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

class Sandbox final : public app::Application {
public:
    void onStart(app::App& app) override {
        render::Device& device = app.device();

        // Geometry (uploaded to GPU buffers) + a texture.
        mesh_ = renderer::uploadMesh(device, render::cubeMesh(0.8f));
        const std::array<u8, 8 * 8 * 4> pixels = checkerTexture();
        texture_ = device.createTexture(8, 8, pixels.data());

        // A pipeline: per-frame camera uniform (binding 0), texture (binding 1),
        // and a per-draw model matrix as a push constant.
        render::PipelineDesc desc;
        desc.vertexSpirv = sandbox::kSceneVertSpirv;
        desc.fragmentSpirv = sandbox::kSceneFragSpirv;
        desc.vertexLayout.stride = sizeof(render::MeshVertex);
        desc.vertexLayout.attributes = {
            {.location = 0, .format = render::VertexFormat::Float32x3, .offset = 0},
            {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 3},
            {.location = 2, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 6},
        };
        desc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
        desc.pushConstantSize = sizeof(math::Mat4);
        Result<render::PipelineHandle> pipeline = device.createPipeline(desc);
        if (pipeline.isErr()) {
            ZUKIRU_LOG_ERROR("sandbox", "pipeline creation failed: {}", pipeline.error().message);
            app.requestQuit();
            return;
        }
        pipeline_ = pipeline.value();

        // Per-frame camera uniform + a bind group holding it and the texture.
        const math::Mat4 identity = math::Mat4::identity();
        camUbo_ = device.createBuffer(render::BufferKind::Uniform, identity.e, sizeof(identity.e));
        const render::BindGroupEntry entries[] = {
            {.binding = 0, .buffer = camUbo_, .texture = {}},
            {.binding = 1, .buffer = {}, .texture = texture_},
        };
        Result<render::BindGroupHandle> group = device.createBindGroup(pipeline_, entries);
        if (group.isErr()) {
            ZUKIRU_LOG_ERROR("sandbox", "bind group failed: {}", group.error().message);
            app.requestQuit();
            return;
        }
        bindGroup_ = group.value();

        // The scene: a formation root with a 4x4 grid of spinning cube children.
        root_ = scene_.createNode("formation");
        constexpr int kGrid = 4;
        for (int gz = 0; gz < kGrid; ++gz) {
            for (int gx = 0; gx < kGrid; ++gx) {
                const f32 x = (static_cast<f32>(gx) - 1.5f) * 1.6f;
                const f32 z = (static_cast<f32>(gz) - 1.5f) * 1.6f;
                const scene::Entity cube = scene_.createNode("cube", root_);
                scene_.setLocalTransform(cube, {.position = {x, 0.0f, z}});
                const math::Vec3 axis = math::normalize(
                    math::Vec3{static_cast<f32>(gx) - 1.5f, 1.0f, static_cast<f32>(gz) - 1.5f});
                const f32 speed = 0.6f + 0.15f * static_cast<f32>(gx + gz);
                // A MeshRenderer makes the node draw itself; the renderer system
                // finds it by (WorldTransform + MeshRenderer).
                scene_.world().add<renderer::MeshRenderer>(
                    cube, {.mesh = mesh_, .pipeline = pipeline_, .bindGroup = bindGroup_});
                scene_.world().add<Spin>(cube, {.axis = axis, .speed = speed});
            }
        }

        if (const char* limit = std::getenv("ZUKIRU_MAX_FRAMES"); limit != nullptr) {
            maxFrames_ = std::strtoull(limit, nullptr, 10);
        }
        ZUKIRU_LOG_INFO("sandbox", "started on {} ({} cubes)", device.deviceName(),
                        scene_.world().entityCount() - 1);
    }

    void onUpdate(app::App& app, f32 dt) override {
        time_ += dt;
        if (maxFrames_ != 0 && app.frameCount() >= maxFrames_) {
            app.requestQuit();
        }

        // --- Orbit camera controls -------------------------------------
        const input::InputState& in = app.input();
        if (in.keyDown(input::Key::Escape)) app.requestQuit();

        const f32 turn = 1.5f * dt;
        const f32 zoom = 6.0f * dt;
        if (in.keyDown(input::Key::A) || in.keyDown(input::Key::Left)) orbitAngle_ -= turn;
        if (in.keyDown(input::Key::D) || in.keyDown(input::Key::Right)) orbitAngle_ += turn;
        if (in.keyDown(input::Key::W) || in.keyDown(input::Key::Up)) orbitRadius_ -= zoom;
        if (in.keyDown(input::Key::S) || in.keyDown(input::Key::Down)) orbitRadius_ += zoom;
        if (in.keyDown(input::Key::Q)) orbitHeight_ += zoom;
        if (in.keyDown(input::Key::E)) orbitHeight_ -= zoom;
        orbitAngle_ += 0.15f * dt;  // gentle auto-rotation
        orbitRadius_ = std::clamp(orbitRadius_, 4.0f, 25.0f);
        orbitHeight_ = std::clamp(orbitHeight_, -8.0f, 12.0f);

        const math::Vec3 eye{std::cos(orbitAngle_) * orbitRadius_, orbitHeight_,
                             std::sin(orbitAngle_) * orbitRadius_};
        camera_.lookAt(eye, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());
        const platform::WindowExtent extent = app.window().extent();
        if (extent.height != 0) {
            camera_.setPerspective(kPi / 3.0f,
                                   static_cast<f32>(extent.width) / static_cast<f32>(extent.height),
                                   0.1f, 100.0f);
        }

        // --- Animate the hierarchy -------------------------------------
        // The whole formation rotates; each cube also spins on its own axis.
        scene_.setLocalTransform(
            root_, {.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), time_ * 0.3f)});
        scene_.world().each<scene::LocalTransform, Spin>(
            [&](scene::LocalTransform& local, const Spin& spin) {
                local.value.rotation = math::Quat::fromAxisAngle(spin.axis, time_ * spin.speed);
            });
        scene_.updateTransforms();
    }

    void onRender(app::App& app) override {
        render::Device& device = app.device();

        // Upload this frame's camera matrix (ring-buffered — safe every frame).
        const math::Mat4 viewProj = camera_.viewProjection();
        device.updateBuffer(camUbo_, viewProj.e, sizeof(viewProj.e));

        // The renderer system draws every MeshRenderer entity, pushing each node's
        // world matrix as a push constant. No hand-wired per-cube draw loop.
        renderer::renderMeshes(device, scene_.world());
    }

    void onShutdown(app::App& app) override {
        // The loop has already idled the GPU before calling us.
        render::Device& device = app.device();
        if (bindGroup_.valid()) device.destroyBindGroup(bindGroup_);
        if (pipeline_.valid()) device.destroyPipeline(pipeline_);
        if (texture_.valid()) device.destroyTexture(texture_);
        if (camUbo_.valid()) device.destroyBuffer(camUbo_);
        renderer::destroyMesh(device, mesh_);
        ZUKIRU_LOG_INFO("sandbox", "shut down after {} frames", app.frameCount());
    }

private:
    renderer::Mesh mesh_{};
    render::BufferHandle camUbo_{};
    render::TextureHandle texture_{};
    render::PipelineHandle pipeline_{};
    render::BindGroupHandle bindGroup_{};

    render::Camera camera_;
    scene::Scene scene_;
    scene::Entity root_{};

    f32 time_ = 0.0f;
    f32 orbitAngle_ = 0.6f;
    f32 orbitHeight_ = 3.5f;
    f32 orbitRadius_ = 9.0f;
    u64 maxFrames_ = 0;  // 0 == run until the window closes
};

}  // namespace

int main() {
    zukiru::Result<std::unique_ptr<zukiru::app::App>> app = zukiru::app::App::create(
        {.window = {.title = "Zukiru Sandbox", .width = 1280, .height = 720}});
    if (app.isErr()) {
        ZUKIRU_LOG_ERROR("sandbox", "failed to start: {}", app.error().message);
        return 1;
    }
    Sandbox game;
    app.value()->run(game);
    return 0;
}
