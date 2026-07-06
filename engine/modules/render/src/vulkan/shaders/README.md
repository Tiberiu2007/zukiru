# Built-in render shaders

GLSL sources for the render module's built-in shaders. They are **compiled to
SPIR-V offline** and embedded in `../triangle_shaders.hpp`, so the render module
needs no shader compiler at build or run time (see ADR 0006 / the shader_compiler
tool).

- `triangle.vert` / `triangle.frag` — the classic vertexless RGB triangle
  (`gl_VertexIndex`-driven), used to prove the graphics-pipeline path.

## Regenerating the embedded header

After editing a shader, rebuild the SPIR-V and re-embed it. From the repo root,
with the shader compiler built (`-DZUKIRU_BUILD_TOOLS=ON`):

```bash
SH=engine/modules/render/src/vulkan/shaders
./build/debug/bin/zukiru-shaderc $SH/triangle.vert -o /tmp/triangle.vert.spv
./build/debug/bin/zukiru-shaderc $SH/triangle.frag -o /tmp/triangle.frag.spv
python3 tools/embed_spirv.py \
    /tmp/triangle.vert.spv /tmp/triangle.frag.spv \
    engine/modules/render/src/vulkan/triangle_shaders.hpp
```

(The `embed_spirv.py` helper packs the SPIR-V words into
`inline constexpr u32 kTriangle*Spirv[]` arrays.)
