// RHI — the render hardware interface: a backend-agnostic handle to the GPU.
//
// Game/scene code talks to this, never to Vulkan directly, so a second backend
// (D3D12/Metal) can drop in later. It supports GPU buffers, graphics pipelines
// built from SPIR-V, and per-frame command recording — enough to draw your own
// geometry. No backend types leak into these headers (see ADR 0006).
//
//   auto device = render::createDevice(*window);
//   auto pipeline = device.value()->createPipeline({
//       .vertexSpirv = vertSpv, .fragmentSpirv = fragSpv,
//       .vertexLayout = {.stride = sizeof(Vertex), .attributes = {...}}});
//   auto vbo = device.value()->createBuffer(render::BufferKind::Vertex, verts, sizeof(verts));
//
//   while (running) {
//       if (!device->beginFrame()) { device->resize(w, h); continue; }
//       device->bindPipeline(pipeline.value());
//       device->bindVertexBuffer(vbo);
//       device->draw(3);
//       device->endFrame();
//   }
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace zukiru::platform {
class Window;
}

namespace zukiru::render {

enum class Backend : u8 {
    Vulkan,
};

// Linear RGBA in [0, 1].
struct Color {
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 1.0f;

    friend constexpr bool operator==(Color, Color) = default;
};

// --- GPU resources (opaque handles owned by the Device) -------------------

struct BufferHandle {
    u32 id = 0;  // 0 == invalid
    [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
    friend constexpr bool operator==(BufferHandle, BufferHandle) = default;
};

struct PipelineHandle {
    u32 id = 0;
    [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
    friend constexpr bool operator==(PipelineHandle, PipelineHandle) = default;
};

enum class BufferKind : u8 {
    Vertex,
    Index,
};

enum class IndexType : u8 {
    U16,
    U32,
};

enum class VertexFormat : u8 {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
};

enum class PrimitiveTopology : u8 {
    TriangleList,
    TriangleStrip,
    LineList,
    PointList,
};

// One vertex attribute: which shader `location` it feeds, its `format`, and its
// byte `offset` within a vertex.
struct VertexAttribute {
    u32 location = 0;
    VertexFormat format = VertexFormat::Float32x3;
    u32 offset = 0;
};

// The layout of one interleaved vertex buffer. Leave `attributes` empty for a
// vertexless pipeline (positions computed from gl_VertexIndex).
struct VertexLayout {
    u32 stride = 0;  // bytes per vertex
    std::vector<VertexAttribute> attributes;
};

// Everything needed to build a graphics pipeline.
struct PipelineDesc {
    std::span<const u32> vertexSpirv;
    std::span<const u32> fragmentSpirv;
    VertexLayout vertexLayout{};
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
};

// --- Device ---------------------------------------------------------------

// A GPU device bound to a window's swapchain.
class Device {
public:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    virtual ~Device() = default;

    // --- Resources -------------------------------------------------------

    // Upload `sizeBytes` of `data` into a new GPU buffer. Returns an invalid
    // handle on failure.
    [[nodiscard]] virtual BufferHandle createBuffer(BufferKind kind, const void* data,
                                                    usize sizeBytes) = 0;
    virtual void destroyBuffer(BufferHandle buffer) = 0;

    // Build a graphics pipeline. Errors on invalid SPIR-V / pipeline creation.
    [[nodiscard]] virtual Result<PipelineHandle> createPipeline(const PipelineDesc& desc) = 0;
    virtual void destroyPipeline(PipelineHandle pipeline) = 0;

    // --- Frame -----------------------------------------------------------

    // Acquire the next swapchain image, clear it, and open command recording.
    // Returns false if the swapchain is out of date — call resize() and retry.
    [[nodiscard]] virtual bool beginFrame() = 0;

    // Close recording, submit, and present.
    virtual void endFrame() = 0;

    // --- Recording (valid only between beginFrame and endFrame) ----------

    virtual void bindPipeline(PipelineHandle pipeline) = 0;
    virtual void bindVertexBuffer(BufferHandle buffer) = 0;
    virtual void bindIndexBuffer(BufferHandle buffer, IndexType type) = 0;
    virtual void draw(u32 vertexCount, u32 firstVertex = 0) = 0;
    virtual void drawIndexed(u32 indexCount, u32 firstIndex = 0, i32 vertexOffset = 0) = 0;

    // --- Misc ------------------------------------------------------------

    virtual void setClearColor(Color color) = 0;
    virtual void resize(u32 width, u32 height) = 0;
    virtual void waitIdle() = 0;
    [[nodiscard]] virtual Backend backend() const = 0;
    [[nodiscard]] virtual std::string_view deviceName() const = 0;

protected:
    Device() = default;
};

struct DeviceConfig {
    Backend backend = Backend::Vulkan;
    bool enableValidation = false;
    bool vsync = true;
};

// Create a device rendering into `window`.
[[nodiscard]] Result<std::unique_ptr<Device>> createDevice(const platform::Window& window,
                                                           const DeviceConfig& config = {});

}  // namespace zukiru::render
