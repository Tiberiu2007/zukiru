#include <zukiru/shaderc/shader_compiler.hpp>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <string>

namespace zukiru::shaderc {
namespace {

// glslang requires one process-wide init. Do it once, lazily, and never finalize
// (process lifetime) — fine for a CLI/library used from a single thread.
void ensureGlslangInitialized() {
    static const bool kInitialized = [] {
        glslang::InitializeProcess();
        return true;
    }();
    (void)kInitialized;
}

[[nodiscard]] EShLanguage toEShLanguage(Stage stage) {
    switch (stage) {
        case Stage::Vertex: return EShLangVertex;
        case Stage::Fragment: return EShLangFragment;
        case Stage::Compute: return EShLangCompute;
        case Stage::Geometry: return EShLangGeometry;
        case Stage::TessControl: return EShLangTessControl;
        case Stage::TessEvaluation: return EShLangTessEvaluation;
    }
    return EShLangVertex;
}

}  // namespace

Result<std::vector<u32>> compileGlslToSpirv(std::string_view source, Stage stage,
                                            const CompileOptions& options) {
    ensureGlslangInitialized();

    const EShLanguage language = toEShLanguage(stage);
    glslang::TShader shader(language);

    const std::string sourceStr{source};
    const char* strings[1] = {sourceStr.c_str()};
    const char* names[1] = {options.sourceName.c_str()};
    const int lengths[1] = {static_cast<int>(sourceStr.size())};
    shader.setStringsWithLengthsAndNames(strings, lengths, names, 1);
    shader.setEntryPoint("main");

    // Target Vulkan 1.1 / SPIR-V 1.3 — matches what the Vulkan backend requests.
    shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    if (options.generateDebugInfo) {
        messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
    }

    constexpr int kDefaultVersion = 100;  // used only if the source omits #version
    if (!shader.parse(GetDefaultResources(), kDefaultVersion, false, messages)) {
        return Err(Error{std::string("shader compile failed:\n") + shader.getInfoLog()});
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        return Err(Error{std::string("shader link failed:\n") + program.getInfoLog()});
    }

    std::vector<unsigned int> words;
    glslang::SpvOptions spvOptions;
    spvOptions.generateDebugInfo = options.generateDebugInfo;
    glslang::GlslangToSpv(*program.getIntermediate(language), words, &spvOptions);

    return Ok(std::vector<u32>(words.begin(), words.end()));
}

std::optional<Stage> stageFromExtension(std::string_view path) {
    const auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return std::nullopt;
    const std::string_view ext = path.substr(dot + 1);
    if (ext == "vert") return Stage::Vertex;
    if (ext == "frag") return Stage::Fragment;
    if (ext == "comp") return Stage::Compute;
    if (ext == "geom") return Stage::Geometry;
    if (ext == "tesc") return Stage::TessControl;
    if (ext == "tese") return Stage::TessEvaluation;
    return std::nullopt;
}

}  // namespace zukiru::shaderc
