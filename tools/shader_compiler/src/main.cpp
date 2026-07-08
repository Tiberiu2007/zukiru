// zuki-shaderc — CLI front-end: compile a GLSL file to a .spv binary.
//
//   zuki-shaderc input.vert -o output.spv [--stage vertex]
//
// The stage is inferred from the input extension (.vert/.frag/...) unless given
// with --stage. SPIR-V is written as raw little-endian 32-bit words.
#include <zuki/shaderc/shader_compiler.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

using namespace zuki;

namespace {

std::optional<shaderc::Stage> parseStage(std::string_view name) {
    if (name == "vertex" || name == "vert") return shaderc::Stage::Vertex;
    if (name == "fragment" || name == "frag") return shaderc::Stage::Fragment;
    if (name == "compute" || name == "comp") return shaderc::Stage::Compute;
    if (name == "geometry" || name == "geom") return shaderc::Stage::Geometry;
    if (name == "tesscontrol" || name == "tesc") return shaderc::Stage::TessControl;
    if (name == "tesseval" || name == "tese") return shaderc::Stage::TessEvaluation;
    return std::nullopt;
}

int usage() {
    std::cerr << "usage: zuki-shaderc <input> -o <output.spv> [--stage <stage>]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string input;
    std::string output;
    std::optional<shaderc::Stage> stage;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg == "--stage" && i + 1 < argc) {
            stage = parseStage(argv[++i]);
            if (!stage) {
                std::cerr << "zuki-shaderc: unknown stage\n";
                return 2;
            }
        } else if (!arg.empty() && arg.front() == '-') {
            return usage();
        } else {
            input = arg;
        }
    }

    if (input.empty() || output.empty()) return usage();

    if (!stage) {
        stage = shaderc::stageFromExtension(input);
        if (!stage) {
            std::cerr << "zuki-shaderc: cannot infer stage from '" << input
                      << "'; pass --stage\n";
            return 2;
        }
    }

    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << "zuki-shaderc: cannot open '" << input << "'\n";
        return 1;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();

    shaderc::CompileOptions options;
    options.sourceName = input;
    const Result<std::vector<u32>> spirv = shaderc::compileGlslToSpirv(source, *stage, options);
    if (spirv.isErr()) {
        std::cerr << spirv.error().message << '\n';
        return 1;
    }

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        std::cerr << "zuki-shaderc: cannot write '" << output << "'\n";
        return 1;
    }
    const std::vector<u32>& words = spirv.value();
    out.write(reinterpret_cast<const char*>(words.data()),
              static_cast<std::streamsize>(words.size() * sizeof(u32)));

    std::cout << "zuki-shaderc: " << input << " -> " << output << " (" << words.size()
              << " words)\n";
    return 0;
}
