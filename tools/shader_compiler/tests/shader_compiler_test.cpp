#include <zuki/shaderc/shader_compiler.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki;
using namespace zuki::shaderc;

namespace {

// The fixed-function fullscreen triangle: no vertex inputs, gl_VertexIndex drives
// three clip-space positions. This is exactly what the render module will use for
// its first triangle.
constexpr const char* kTriangleVert = R"(#version 450
vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
void main() { gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); }
)";

constexpr const char* kSolidFrag = R"(#version 450
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0, 0.4, 0.1, 1.0); }
)";

constexpr u32 kSpirvMagic = 0x07230203u;

}  // namespace

TEST_CASE("compiles a vertex shader to valid SPIR-V", "[shaderc]") {
    const Result<std::vector<u32>> spirv = compileGlslToSpirv(kTriangleVert, Stage::Vertex);
    REQUIRE(spirv.isOk());
    REQUIRE_FALSE(spirv.value().empty());
    REQUIRE(spirv.value().front() == kSpirvMagic);  // SPIR-V header magic number
}

TEST_CASE("compiles a fragment shader to valid SPIR-V", "[shaderc]") {
    const Result<std::vector<u32>> spirv = compileGlslToSpirv(kSolidFrag, Stage::Fragment);
    REQUIRE(spirv.isOk());
    REQUIRE(spirv.value().front() == kSpirvMagic);
}

TEST_CASE("invalid GLSL fails with a diagnostic", "[shaderc]") {
    const Result<std::vector<u32>> spirv =
        compileGlslToSpirv("#version 450\nvoid main() { this is not glsl }", Stage::Vertex);
    REQUIRE(spirv.isErr());
    REQUIRE_FALSE(spirv.error().message.empty());
}

TEST_CASE("debug info makes the module larger but still valid", "[shaderc]") {
    const Result<std::vector<u32>> lean = compileGlslToSpirv(kTriangleVert, Stage::Vertex);
    CompileOptions withDebug;
    withDebug.generateDebugInfo = true;
    const Result<std::vector<u32>> fat = compileGlslToSpirv(kTriangleVert, Stage::Vertex, withDebug);

    REQUIRE(lean.isOk());
    REQUIRE(fat.isOk());
    REQUIRE(fat.value().front() == kSpirvMagic);
    REQUIRE(fat.value().size() > lean.value().size());
}

TEST_CASE("stage is inferred from file extension", "[shaderc]") {
    REQUIRE(stageFromExtension("hero.vert") == Stage::Vertex);
    REQUIRE(stageFromExtension("path/to/water.frag") == Stage::Fragment);
    REQUIRE(stageFromExtension("blur.comp") == Stage::Compute);
    REQUIRE(stageFromExtension("mesh.tese") == Stage::TessEvaluation);
    REQUIRE(stageFromExtension("noext") == std::nullopt);
    REQUIRE(stageFromExtension("model.png") == std::nullopt);
}
