# shader_compiler

**Offline tool (`tools/`).** Compiles GLSL to SPIR-V. Two pieces:

- **`zuki::shaderc`** — a small library: `compileGlslToSpirv(source, stage)` →
  `Result<std::vector<u32>>`.
- **`zuki-shaderc`** — a CLI front-end that turns a `.vert`/`.frag`/… file into
  a `.spv` binary.

It wraps [glslang](https://github.com/KhronosGroup/glslang) (fetched and built via
CMake — no system SPIR-V toolchain is assumed). glslang is a **private** impl
detail: the public header exposes no glslang types, so shaders are cooked
**offline** and the runtime consumes SPIR-V without the compiler.

## Building

The tool is opt-in (it pulls in glslang):

```bash
cmake --preset debug -DZUKI_BUILD_TOOLS=ON
cmake --build build/debug --target zuki-shaderc
```

## CLI

```bash
zuki-shaderc triangle.vert -o triangle.vert.spv        # stage from extension
zuki-shaderc shader.glsl   -o out.spv --stage fragment # or explicit
```

Exits non-zero and prints glslang's diagnostics on a compile error. SPIR-V is
written as raw little-endian 32-bit words (first word is the magic `0x07230203`).

## Library

```cpp
#include <zuki/shaderc/shader_compiler.hpp>
using namespace zuki;

auto spirv = shaderc::compileGlslToSpirv(source, shaderc::Stage::Vertex);
if (spirv) uploadModule(spirv.value());
else       log(spirv.error().message);   // parse/link log
```

Targets **Vulkan 1.1 / SPIR-V 1.3**, matching the [`render`](../../engine/modules/render)
backend. `CompileOptions::generateDebugInfo` keeps `OpSource`/`OpLine`.
`stageFromExtension()` maps `.vert/.frag/.comp/.geom/.tesc/.tese`.

## Why this exists

It unblocks the render module's **first triangle**: a graphics pipeline needs
SPIR-V shader modules, and this is how they're produced. Later it becomes part of
the offline asset pipeline (alongside `asset_cooker`), cooking shaders into
packages so shipped builds never link a compiler.

## Scope

MVP: single-stage GLSL → SPIR-V, Vulkan target. Deferred: `#include` resolution,
preprocessor defines, reflection extraction, SPIR-V optimization
(`ENABLE_OPT` is off), HLSL, and shader hot-reload.

## Tests

```bash
ctest --preset debug -R '^shaderc\.'   # requires -DZUKI_BUILD_TOOLS=ON
```

Compiles the actual fullscreen-triangle vertex + solid-color fragment shaders to
valid SPIR-V (verifies the magic word), checks the debug-info path, exercises the
error path, and covers extension→stage inference.
