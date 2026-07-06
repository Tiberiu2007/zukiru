#include <zukiru/render/primitives.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace zukiru;
using namespace zukiru::render;

TEST_CASE("cubeMesh has 6 quad faces of independent vertices", "[render][primitives]") {
    const MeshData cube = cubeMesh();
    REQUIRE(cube.vertices.size() == 24);   // 4 per face, flat normals
    REQUIRE(cube.indices.size() == 36);    // 2 triangles per face
    REQUIRE(cube.vertexBytes() == 24 * sizeof(MeshVertex));
    REQUIRE(cube.indexBytes() == 36 * sizeof(u16));
    REQUIRE(sizeof(MeshVertex) == 32);     // pos(12) + normal(12) + uv(8)
}

TEST_CASE("cubeMesh indices stay in range", "[render][primitives]") {
    const MeshData cube = cubeMesh();
    for (const u16 index : cube.indices) {
        REQUIRE(index < cube.vertices.size());
    }
}

TEST_CASE("cubeMesh spans [-size/2, size/2] on every axis", "[render][primitives]") {
    const MeshData cube = cubeMesh(2.0f);
    for (const MeshVertex& v : cube.vertices) {
        REQUIRE(std::abs(v.position[0]) == 1.0f);
        REQUIRE(std::abs(v.position[1]) == 1.0f);
        REQUIRE(std::abs(v.position[2]) == 1.0f);
        // Each face normal is a unit axis vector.
        const f32 len = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] +
                                  v.normal[2] * v.normal[2]);
        REQUIRE(len == 1.0f);
    }
}
