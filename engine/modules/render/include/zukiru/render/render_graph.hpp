// Render graph — a frame organizer. Declare passes and the resources they read and
// write; `compile()` orders them by dependency (rejecting cycles) and drops passes
// whose results nothing consumes; `execute()` runs the live passes in order.
//
// Backend-agnostic: built on the RHI, no Vulkan types (see ADR 0008). Physical
// allocation of transient resources and automatic barriers are deferred until the
// RHI gains offscreen render targets — today every pass renders to the current
// framebuffer, so the graph's job is ordering and culling.
//
//   RenderGraph graph;
//   const RgResource shadow = graph.createResource("shadowMap");
//   const RgResource color  = graph.importResource("backbuffer");  // external
//   graph.addPass("shadow").writes(shadow).setExecute([&](PassContext& c) { ... });
//   graph.addPass("opaque").reads(shadow).writes(color).setExecute([&](PassContext& c) { ... });
//   if (auto compiled = graph.compile(); compiled) {
//       if (device.beginFrame()) { graph.execute(device); device.endFrame(); }
//   }
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::render {

class Device;
class RenderGraph;

// A virtual resource (a render target / buffer) produced and consumed by passes.
struct RgResource {
    u32 id = 0;  // 0 == invalid
    [[nodiscard]] constexpr bool valid() const noexcept { return id != 0; }
    friend constexpr bool operator==(RgResource, RgResource) = default;
};

// Passed to a pass's execute callback while the graph runs. Holds the device the
// pass records into; a home for per-pass render-target/viewport state as the RHI
// grows.
struct PassContext {
    Device& device;
};

using PassExecuteFn = std::function<void(PassContext&)>;

// Fluent builder for one pass. Returned by `RenderGraph::addPass`; the references
// stay valid until the next `addPass` / `compile`.
class PassBuilder {
public:
    PassBuilder& reads(RgResource resource);
    PassBuilder& writes(RgResource resource);
    PassBuilder& setExecute(PassExecuteFn fn);

private:
    friend class RenderGraph;
    PassBuilder(RenderGraph& graph, u32 passIndex) : graph_(&graph), passIndex_(passIndex) {}

    RenderGraph* graph_;
    u32 passIndex_;
};

// The compiled schedule: live passes in execution order (indices into the graph's
// passes). Produced by `compile`, consumed by `execute`.
struct CompiledGraph {
    std::vector<u32> order;  // pass indices, dependency-sorted, dead passes removed
};

class RenderGraph {
public:
    RenderGraph() = default;

    // Declare a transient resource the graph owns.
    [[nodiscard]] RgResource createResource(std::string name);
    // Declare an external resource (e.g. the backbuffer). Passes writing an
    // imported resource are always live — they have observable side effects.
    [[nodiscard]] RgResource importResource(std::string name);

    // Begin a new pass. Chain reads/writes/setExecute on the returned builder.
    [[nodiscard]] PassBuilder addPass(std::string name);

    // Order passes by dependency and cull dead ones. Errors on a dependency cycle
    // or a pass that reads a resource no pass writes (a dangling read).
    [[nodiscard]] Result<CompiledGraph> compile() const;

    // Run the compiled passes in order, calling each one's execute callback.
    void execute(Device& device, const CompiledGraph& compiled) const;
    // Convenience: compile then execute. Silently does nothing if compilation
    // fails (use the two-step form when you need the error).
    void execute(Device& device) const;

    [[nodiscard]] usize passCount() const noexcept { return passes_.size(); }
    [[nodiscard]] usize resourceCount() const noexcept { return resources_.size(); }

private:
    friend class PassBuilder;

    struct Pass {
        std::string name;
        std::vector<u32> reads;   // resource ids
        std::vector<u32> writes;  // resource ids
        PassExecuteFn execute;
    };
    struct Resource {
        std::string name;
        bool imported = false;
    };

    Pass& passAt(u32 index) { return passes_[index]; }

    std::vector<Pass> passes_;
    std::vector<Resource> resources_;  // index i is resource id i+1
};

}  // namespace zukiru::render
