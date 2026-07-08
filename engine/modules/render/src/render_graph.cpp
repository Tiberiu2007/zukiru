// Render graph compilation + execution. The scheduling core: dependency ordering,
// cycle detection, and dead-pass culling over the declared pass/resource DAG.
// See ADR 0008. Physical transient-resource allocation is deferred to a future
// render-target RHI; today passes record into the current framebuffer.
#include <zuki/render/render_graph.hpp>

#include <unordered_set>
#include <utility>

namespace zuki::render {

// --- PassBuilder ---------------------------------------------------------

PassBuilder& PassBuilder::reads(RgResource resource) {
    if (resource.valid()) graph_->passAt(passIndex_).reads.push_back(resource.id);
    return *this;
}

PassBuilder& PassBuilder::writes(RgResource resource) {
    if (resource.valid()) graph_->passAt(passIndex_).writes.push_back(resource.id);
    return *this;
}

PassBuilder& PassBuilder::setExecute(PassExecuteFn fn) {
    graph_->passAt(passIndex_).execute = std::move(fn);
    return *this;
}

// --- RenderGraph builders ------------------------------------------------

RgResource RenderGraph::createResource(std::string name) {
    resources_.push_back({std::move(name), /*imported=*/false});
    return RgResource{static_cast<u32>(resources_.size())};  // id = index + 1
}

RgResource RenderGraph::importResource(std::string name) {
    resources_.push_back({std::move(name), /*imported=*/true});
    return RgResource{static_cast<u32>(resources_.size())};
}

PassBuilder RenderGraph::addPass(std::string name) {
    const auto index = static_cast<u32>(passes_.size());
    passes_.push_back(Pass{std::move(name), {}, {}, {}});
    return PassBuilder{*this, index};
}

// --- Compile -------------------------------------------------------------

Result<CompiledGraph> RenderGraph::compile() const {
    const auto passCount = static_cast<u32>(passes_.size());

    // Writers of each resource (indexed by resource id; slot 0 unused).
    std::vector<std::vector<u32>> writers(resources_.size() + 1);
    for (u32 p = 0; p < passCount; ++p) {
        for (const u32 r : passes_[p].writes) writers[r].push_back(p);
    }

    // A read of a non-imported resource that nothing writes is a dangling read.
    for (u32 p = 0; p < passCount; ++p) {
        for (const u32 r : passes_[p].reads) {
            if (!resources_[r - 1].imported && writers[r].empty()) {
                return Err(Error{"render graph: pass '" + passes_[p].name +
                                 "' reads unwritten resource '" + resources_[r - 1].name + "'"});
            }
        }
    }

    // Liveness: roots are passes that write an imported (externally observed)
    // resource; a pass is live if it feeds a live pass. Reach backward from the
    // roots along read → writer edges.
    std::vector<bool> needed(passCount, false);
    std::vector<u32> stack;
    for (u32 p = 0; p < passCount; ++p) {
        for (const u32 r : passes_[p].writes) {
            if (resources_[r - 1].imported) {
                needed[p] = true;
                stack.push_back(p);
                break;
            }
        }
    }
    while (!stack.empty()) {
        const u32 p = stack.back();
        stack.pop_back();
        for (const u32 r : passes_[p].reads) {
            for (const u32 w : writers[r]) {
                if (!needed[w]) {
                    needed[w] = true;
                    stack.push_back(w);
                }
            }
        }
    }

    // Read-after-write edges among live passes (deduped): writer → reader.
    std::vector<std::vector<u32>> successors(passCount);
    std::vector<u32> indegree(passCount, 0);
    std::unordered_set<u64> seen;
    for (u32 p = 0; p < passCount; ++p) {
        if (!needed[p]) continue;
        for (const u32 r : passes_[p].reads) {
            for (const u32 w : writers[r]) {
                if (w == p || !needed[w]) continue;
                const u64 key = (static_cast<u64>(w) << 32) | p;
                if (seen.insert(key).second) {
                    successors[w].push_back(p);
                    ++indegree[p];
                }
            }
        }
    }

    // Kahn's algorithm; pick the lowest index among ready passes for a stable order.
    u32 neededCount = 0;
    for (u32 p = 0; p < passCount; ++p) {
        if (needed[p]) ++neededCount;
    }
    CompiledGraph out;
    out.order.reserve(neededCount);
    std::vector<bool> done(passCount, false);
    while (out.order.size() < neededCount) {
        i32 pick = -1;
        for (u32 p = 0; p < passCount; ++p) {
            if (needed[p] && !done[p] && indegree[p] == 0) {
                pick = static_cast<i32>(p);
                break;
            }
        }
        if (pick < 0) {
            return Err(Error{"render graph: dependency cycle among passes"});
        }
        const auto picked = static_cast<u32>(pick);
        done[picked] = true;
        out.order.push_back(picked);
        for (const u32 to : successors[picked]) --indegree[to];
    }
    return Ok(std::move(out));
}

// --- Execute -------------------------------------------------------------

void RenderGraph::execute(Device& device, const CompiledGraph& compiled) const {
    PassContext context{device};
    for (const u32 index : compiled.order) {
        const Pass& pass = passes_[index];
        if (pass.execute) pass.execute(context);
    }
}

void RenderGraph::execute(Device& device) const {
    Result<CompiledGraph> compiled = compile();
    if (compiled.isOk()) execute(device, compiled.value());
}

}  // namespace zuki::render
