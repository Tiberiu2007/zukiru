#!/usr/bin/env python3
"""Embed SPIR-V binaries into a C++ header of `inline constexpr u32` arrays.

Usage:
    embed_spirv.py <vert.spv> <frag.spv> <out.hpp>

Packs the vertex module as `kTriangleVertSpirv` and the fragment module as
`kTriangleFragSpirv`. Used to regenerate engine/modules/render/src/vulkan/
triangle_shaders.hpp after editing the built-in shaders (see that dir's README).
"""
import struct
import sys


def read_words(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % 4 != 0:
        raise SystemExit(f"{path}: SPIR-V is not 32-bit-word aligned")
    return list(struct.unpack("<%dI" % (len(data) // 4), data))


def emit_array(name, words):
    lines = [f"inline constexpr u32 {name}[] = {{"]
    for i in range(0, len(words), 6):
        chunk = words[i:i + 6]
        lines.append("    " + ", ".join(f"0x{w:08x}u" for w in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    vert, frag, out = sys.argv[1], sys.argv[2], sys.argv[3]
    v, f = read_words(vert), read_words(frag)
    header = (
        "// GENERATED — do not edit. The built-in RGB triangle shaders, compiled from\n"
        "// src/vulkan/shaders/triangle.{vert,frag} to SPIR-V by zuki-shaderc and embedded\n"
        "// here so the render module needs no shader compiler at build or run time.\n"
        "//\n"
        "// Regenerate (from repo root, with -DZUKI_BUILD_TOOLS=ON built) via the recipe\n"
        "// in src/vulkan/shaders/README.md.\n"
        "#pragma once\n\n"
        "#include <zuki/core/types.hpp>\n\n"
        "namespace zuki::render {\n\n"
        f"{emit_array('kTriangleVertSpirv', v)}\n\n"
        f"{emit_array('kTriangleFragSpirv', f)}\n\n"
        "}  // namespace zuki::render\n"
    )
    with open(out, "w") as o:
        o.write(header)
    print(f"embed_spirv: vert={len(v)} words, frag={len(f)} words -> {out}")


if __name__ == "__main__":
    main()
