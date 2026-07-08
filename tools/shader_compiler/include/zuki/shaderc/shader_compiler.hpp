// Offline shader compiler: GLSL source -> SPIR-V words.
//
// A thin, backend-agnostic wrapper over glslang. The public surface exposes no
// glslang types, so the render/asset pipeline can consume SPIR-V without pulling
// in the compiler at runtime (the CLI `zuki-shaderc` cooks shaders offline).
//
//   auto spirv = shaderc::compileGlslToSpirv(source, shaderc::Stage::Vertex);
//   if (spirv) uploadToPipeline(spirv.value());
//   else       log(spirv.error().message);
#pragma once

#include <zuki/core/result.hpp>
#include <zuki/core/types.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zuki::shaderc {

enum class Stage : u8 {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation,
};

struct CompileOptions {
    bool generateDebugInfo = false;   // keep OpSource/OpLine debug instructions
    std::string sourceName = "shader";  // shown in diagnostics
};

// Compile GLSL `source` for `stage` to SPIR-V words. On failure the Error carries
// glslang's info log (parse/link diagnostics).
[[nodiscard]] Result<std::vector<u32>> compileGlslToSpirv(std::string_view source, Stage stage,
                                                          const CompileOptions& options = {});

// Infer the shader stage from a file extension (.vert/.frag/.comp/.geom/.tesc/.tese).
[[nodiscard]] std::optional<Stage> stageFromExtension(std::string_view path);

}  // namespace zuki::shaderc
