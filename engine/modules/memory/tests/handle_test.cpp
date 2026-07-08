#include <zuki/memory/handle.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace zuki::memory;

namespace {
struct MeshTag;
struct TextureTag;
using MeshHandle = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;
}  // namespace

TEST_CASE("default handle is invalid", "[memory][handle]") {
    constexpr MeshHandle h;
    STATIC_REQUIRE_FALSE(h.isValid());
    STATIC_REQUIRE(h == MeshHandle::invalid());
}

TEST_CASE("a handle with a real index is valid", "[memory][handle]") {
    constexpr MeshHandle h{5, 2};
    STATIC_REQUIRE(h.isValid());
    STATIC_REQUIRE(h.index == 5);
    STATIC_REQUIRE(h.generation == 2);
}

TEST_CASE("equality compares index and generation", "[memory][handle]") {
    STATIC_REQUIRE(MeshHandle{1, 0} == MeshHandle{1, 0});
    STATIC_REQUIRE_FALSE(MeshHandle{1, 0} == MeshHandle{1, 1});  // stale generation differs
    STATIC_REQUIRE_FALSE(MeshHandle{1, 0} == MeshHandle{2, 0});
}

TEST_CASE("tags make handle types distinct", "[memory][handle]") {
    STATIC_REQUIRE_FALSE(std::is_same_v<MeshHandle, TextureHandle>);
    STATIC_REQUIRE(sizeof(MeshHandle) == 8);
}
