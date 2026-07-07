// Materials — a shader pipeline plus its named parameter values, as one bindable
// thing. Built on the RHI (pipelines + uniform buffers + textures + bind groups);
// no backend types leak (see ADR 0008).
//
// Split so the packing logic is GPU-free and unit-testable:
//   MaterialLayout  — the schema: named std140 uniform members + texture slots,
//                     and the RHI `bindings()` a matching shader declares.
//   MaterialParams  — a CPU byte block filled through the layout (set*/data).
//   MaterialTemplate— owns the pipeline for a layout (shared across instances).
//   Material        — a template instance: uniform buffer + textures + bind group.
//
//   MaterialLayout layout;
//   layout.addMat4("mvp").addMat4("model").addTexture("tex");
//   auto tmpl = MaterialTemplate::create(device, {.layout = layout,
//       .vertexSpirv = vs, .fragmentSpirv = fs, .vertexLayout = vl}).value();
//   auto mat = tmpl->instantiate();
//   mat->setMat4("mvp", mvp).setTexture("tex", albedo);
//   // per frame, inside a render pass: mat->bind(device); device.draw(...);
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>
#include <zukiru/math/mat.hpp>
#include <zukiru/math/vec.hpp>
#include <zukiru/render/rhi.hpp>

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::render {

// A uniform member's scalar/vector/matrix type.
enum class ParamType : u8 {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
};

// std140 byte size of a uniform member (vec3 occupies 12, but aligns to 16).
[[nodiscard]] constexpr u32 paramSize(ParamType type) noexcept {
    switch (type) {
        case ParamType::Float: return 4;
        case ParamType::Vec2: return 8;
        case ParamType::Vec3: return 12;
        case ParamType::Vec4: return 16;
        case ParamType::Mat4: return 64;
    }
    return 0;
}

// std140 alignment of a uniform member.
[[nodiscard]] constexpr u32 paramAlign(ParamType type) noexcept {
    switch (type) {
        case ParamType::Float: return 4;
        case ParamType::Vec2: return 8;
        case ParamType::Vec3: return 16;
        case ParamType::Vec4: return 16;
        case ParamType::Mat4: return 16;
    }
    return 4;
}

// The schema of a material: a std140 uniform block of named members plus named
// texture slots. Pure — computes offsets, block size, and RHI bindings, with no
// GPU objects. Binding 0 is the uniform block (when there are params); textures
// follow in declaration order.
class MaterialLayout {
public:
    struct Param {
        std::string name;
        ParamType type = ParamType::Float;
        u32 offset = 0;  // std140 byte offset within the uniform block
    };
    struct TextureSlot {
        std::string name;
    };

    MaterialLayout& addParam(std::string name, ParamType type) {
        const u32 aligned = align(uniformCursor_, paramAlign(type));
        params_.push_back({std::move(name), type, aligned});
        uniformCursor_ = aligned + paramSize(type);
        return *this;
    }
    MaterialLayout& addFloat(std::string name) { return addParam(std::move(name), ParamType::Float); }
    MaterialLayout& addVec2(std::string name) { return addParam(std::move(name), ParamType::Vec2); }
    MaterialLayout& addVec3(std::string name) { return addParam(std::move(name), ParamType::Vec3); }
    MaterialLayout& addVec4(std::string name) { return addParam(std::move(name), ParamType::Vec4); }
    MaterialLayout& addMat4(std::string name) { return addParam(std::move(name), ParamType::Mat4); }
    MaterialLayout& addTexture(std::string name) {
        textures_.push_back({std::move(name)});
        return *this;
    }

    [[nodiscard]] const std::vector<Param>& params() const noexcept { return params_; }
    [[nodiscard]] const std::vector<TextureSlot>& textures() const noexcept { return textures_; }

    // Total uniform block size, padded to 16; 0 when there are no params.
    [[nodiscard]] u32 uniformSize() const noexcept {
        return params_.empty() ? 0u : align(uniformCursor_, 16);
    }
    [[nodiscard]] bool hasUniformBlock() const noexcept { return !params_.empty(); }

    [[nodiscard]] const Param* findParam(std::string_view name) const noexcept {
        for (const Param& p : params_) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }
    // Binding index for a texture slot; -1 if not found.
    [[nodiscard]] i32 textureBinding(std::string_view name) const noexcept {
        const u32 base = hasUniformBlock() ? 1u : 0u;
        for (usize i = 0; i < textures_.size(); ++i) {
            if (textures_[i].name == name) return static_cast<i32>(base + i);
        }
        return -1;
    }

    // The RHI binding list matching a shader: index i is binding i.
    [[nodiscard]] std::vector<BindingType> bindings() const {
        std::vector<BindingType> result;
        if (hasUniformBlock()) result.push_back(BindingType::UniformBuffer);
        for (usize i = 0; i < textures_.size(); ++i) result.push_back(BindingType::Texture);
        return result;
    }

private:
    static constexpr u32 align(u32 value, u32 alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    std::vector<Param> params_;
    std::vector<TextureSlot> textures_;
    u32 uniformCursor_ = 0;  // unpadded running size
};

// A CPU-side value: a layout paired with a byte block written through named
// setters. GPU-free — the bytes are what a Material uploads to its uniform buffer.
class MaterialParams {
public:
    MaterialParams() = default;
    explicit MaterialParams(MaterialLayout layout)
        : layout_(std::move(layout)), block_(layout_.uniformSize(), u8{0}) {}

    MaterialParams& setFloat(std::string_view name, f32 value) {
        write(name, ParamType::Float, &value, sizeof(value));
        return *this;
    }
    MaterialParams& setVec2(std::string_view name, math::Vec2 value) {
        const f32 v[2] = {value.x, value.y};
        write(name, ParamType::Vec2, v, sizeof(v));
        return *this;
    }
    MaterialParams& setVec3(std::string_view name, math::Vec3 value) {
        const f32 v[3] = {value.x, value.y, value.z};
        write(name, ParamType::Vec3, v, sizeof(v));
        return *this;
    }
    MaterialParams& setVec4(std::string_view name, math::Vec4 value) {
        const f32 v[4] = {value.x, value.y, value.z, value.w};
        write(name, ParamType::Vec4, v, sizeof(v));
        return *this;
    }
    MaterialParams& setMat4(std::string_view name, const math::Mat4& value) {
        write(name, ParamType::Mat4, value.e, sizeof(value.e));
        return *this;
    }

    [[nodiscard]] const MaterialLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] std::span<const u8> data() const noexcept { return block_; }
    [[nodiscard]] usize size() const noexcept { return block_.size(); }

private:
    // Copy `bytes` into the block at the named param's offset, when the type matches.
    void write(std::string_view name, ParamType type, const void* bytes, usize count) {
        const MaterialLayout::Param* param = layout_.findParam(name);
        if (param == nullptr || param->type != type) return;
        if (param->offset + count > block_.size()) return;
        std::memcpy(block_.data() + param->offset, bytes, count);
    }

    MaterialLayout layout_;
    std::vector<u8> block_;
};

class Material;

// Everything needed to build a material's pipeline.
struct MaterialTemplateDesc {
    MaterialLayout layout;
    std::span<const u32> vertexSpirv;
    std::span<const u32> fragmentSpirv;
    VertexLayout vertexLayout{};
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool depthTest = true;
    bool depthWrite = true;
};

// Owns the pipeline for a material layout; instances share it. Bound to the
// device it was created on.
class MaterialTemplate {
public:
    MaterialTemplate(const MaterialTemplate&) = delete;
    MaterialTemplate& operator=(const MaterialTemplate&) = delete;
    ~MaterialTemplate();

    [[nodiscard]] static Result<std::unique_ptr<MaterialTemplate>> create(
        Device& device, const MaterialTemplateDesc& desc);

    // A fresh instance with its own uniform buffer / textures / bind group.
    [[nodiscard]] std::unique_ptr<Material> instantiate();

    [[nodiscard]] const MaterialLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] PipelineHandle pipeline() const noexcept { return pipeline_; }
    [[nodiscard]] Device& device() const noexcept { return *device_; }

private:
    MaterialTemplate(Device& device, MaterialLayout layout, PipelineHandle pipeline)
        : device_(&device), layout_(std::move(layout)), pipeline_(pipeline) {}

    Device* device_;
    MaterialLayout layout_;
    PipelineHandle pipeline_;
};

// A material instance: parameter values plus the GPU resources that back them.
// `set*` mutate the values; `bind` uploads any changes and records the draw state.
class Material {
public:
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    ~Material();

    Material& setFloat(std::string_view name, f32 value) {
        params_.setFloat(name, value);
        return *this;
    }
    Material& setVec2(std::string_view name, math::Vec2 value) {
        params_.setVec2(name, value);
        return *this;
    }
    Material& setVec3(std::string_view name, math::Vec3 value) {
        params_.setVec3(name, value);
        return *this;
    }
    Material& setVec4(std::string_view name, math::Vec4 value) {
        params_.setVec4(name, value);
        return *this;
    }
    Material& setMat4(std::string_view name, const math::Mat4& value) {
        params_.setMat4(name, value);
        return *this;
    }
    Material& setTexture(std::string_view name, TextureHandle texture);

    // Upload the parameters (ring-buffered, so safe every frame), (re)build the bind
    // group if textures changed, then record bindPipeline + bindBindGroup. Call
    // inside a render pass.
    void bind(Device& device);

    [[nodiscard]] const MaterialParams& params() const noexcept { return params_; }

private:
    friend class MaterialTemplate;
    Material(MaterialTemplate& tmpl, BufferHandle uniformBuffer);

    MaterialTemplate* template_;
    MaterialParams params_;
    BufferHandle uniformBuffer_{};          // invalid when the layout has no params
    std::vector<TextureHandle> textures_;   // one per layout texture slot
    BindGroupHandle bindGroup_{};
    bool bindGroupDirty_ = true;
};

}  // namespace zukiru::render
