#include <zukiru/platform/dynamic_library.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;
using namespace zukiru::platform;

TEST_CASE("loading the current process resolves a runtime symbol",
          "[platform][dylib]") {
    DynamicLibrary self;
    REQUIRE(self.load(""));  // empty path => symbols from this process
    REQUIRE(self.isLoaded());

    // `malloc` is always present in the C runtime.
    REQUIRE(self.getSymbol("malloc") != nullptr);
    REQUIRE(self.getSymbol("a_symbol_that_certainly_does_not_exist_zzz") == nullptr);
}

TEST_CASE("loading a nonexistent library fails cleanly", "[platform][dylib]") {
    DynamicLibrary lib;
    REQUIRE_FALSE(lib.load("/no/such/path/libdoesnotexist.so"));
    REQUIRE_FALSE(lib.isLoaded());
    REQUIRE(lib.getSymbol("anything") == nullptr);
}

TEST_CASE("getFunction returns a callable pointer", "[platform][dylib]") {
    DynamicLibrary self;
    REQUIRE(self.load(""));
    using StrlenFn = unsigned long (*)(const char*);
    auto fn = self.getFunction<StrlenFn>("strlen");
    REQUIRE(fn != nullptr);
    REQUIRE(fn("zukiru") == 6);
}

TEST_CASE("native extension is platform-appropriate", "[platform][dylib]") {
    const auto ext = DynamicLibrary::nativeExtension();
    REQUIRE((ext == ".so" || ext == ".dll" || ext == ".dylib"));
}

TEST_CASE("DynamicLibrary is movable and unloads", "[platform][dylib]") {
    DynamicLibrary a;
    REQUIRE(a.load(""));
    DynamicLibrary b(std::move(a));
    REQUIRE(b.isLoaded());
    b.unload();
    REQUIRE_FALSE(b.isLoaded());
}
