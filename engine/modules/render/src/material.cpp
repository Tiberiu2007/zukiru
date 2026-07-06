// Material system: a pipeline (MaterialTemplate) plus its parameter values and
// GPU resources (Material), built entirely on the RHI. See ADR 0008.
#include <zukiru/render/material.hpp>

namespace zukiru::render {

// --- MaterialTemplate ----------------------------------------------------

Result<std::unique_ptr<MaterialTemplate>> MaterialTemplate::create(
    Device& device, const MaterialTemplateDesc& desc) {
    PipelineDesc pipelineDesc;
    pipelineDesc.vertexSpirv = desc.vertexSpirv;
    pipelineDesc.fragmentSpirv = desc.fragmentSpirv;
    pipelineDesc.vertexLayout = desc.vertexLayout;
    pipelineDesc.topology = desc.topology;
    pipelineDesc.bindings = desc.layout.bindings();
    pipelineDesc.depthTest = desc.depthTest;
    pipelineDesc.depthWrite = desc.depthWrite;

    Result<PipelineHandle> pipeline = device.createPipeline(pipelineDesc);
    if (pipeline.isErr()) {
        return Err(std::move(pipeline.error()));
    }
    // Private ctor — can't use make_unique.
    return Ok(std::unique_ptr<MaterialTemplate>(
        new MaterialTemplate(device, desc.layout, pipeline.value())));
}

MaterialTemplate::~MaterialTemplate() {
    if (pipeline_.valid()) device_->destroyPipeline(pipeline_);
}

std::unique_ptr<Material> MaterialTemplate::instantiate() {
    BufferHandle uniformBuffer{};
    if (layout_.hasUniformBlock()) {
        // Zero-initialised; the first bind() uploads the real values.
        uniformBuffer =
            device_->createBuffer(BufferKind::Uniform, nullptr, layout_.uniformSize());
    }
    return std::unique_ptr<Material>(new Material(*this, uniformBuffer));
}

// --- Material ------------------------------------------------------------

Material::Material(MaterialTemplate& tmpl, BufferHandle uniformBuffer)
    : template_(&tmpl),
      params_(tmpl.layout()),
      uniformBuffer_(uniformBuffer),
      textures_(tmpl.layout().textures().size()) {}

Material::~Material() {
    Device& device = template_->device();
    if (bindGroup_.valid()) device.destroyBindGroup(bindGroup_);
    if (uniformBuffer_.valid()) device.destroyBuffer(uniformBuffer_);
}

Material& Material::setTexture(std::string_view name, TextureHandle texture) {
    const MaterialLayout& layout = template_->layout();
    const std::vector<MaterialLayout::TextureSlot>& slots = layout.textures();
    for (usize i = 0; i < slots.size(); ++i) {
        if (slots[i].name == name) {
            textures_[i] = texture;
            bindGroupDirty_ = true;
            break;
        }
    }
    return *this;
}

void Material::bind(Device& device) {
    const MaterialLayout& layout = template_->layout();

    if (uniformDirty_ && uniformBuffer_.valid()) {
        device.updateBuffer(uniformBuffer_, params_.data().data(), params_.size());
        uniformDirty_ = false;
    }

    // (Re)build the bind group once every declared texture slot is filled.
    if (bindGroupDirty_ && !layout.bindings().empty()) {
        bool allTexturesSet = true;
        for (const TextureHandle texture : textures_) {
            if (!texture.valid()) allTexturesSet = false;
        }
        if (allTexturesSet) {
            std::vector<BindGroupEntry> entries;
            entries.reserve(layout.bindings().size());
            if (layout.hasUniformBlock()) {
                entries.push_back({.binding = 0, .buffer = uniformBuffer_, .texture = {}});
            }
            const u32 base = layout.hasUniformBlock() ? 1u : 0u;
            for (usize i = 0; i < textures_.size(); ++i) {
                entries.push_back({.binding = static_cast<u32>(base + i),
                                   .buffer = {},
                                   .texture = textures_[i]});
            }
            Result<BindGroupHandle> group = device.createBindGroup(template_->pipeline(), entries);
            if (group.isOk()) {
                if (bindGroup_.valid()) device.destroyBindGroup(bindGroup_);
                bindGroup_ = group.value();
                bindGroupDirty_ = false;
            }
        }
    }

    device.bindPipeline(template_->pipeline());
    if (bindGroup_.valid()) device.bindBindGroup(bindGroup_);
}

}  // namespace zukiru::render
