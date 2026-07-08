#include <zuki/core/assert.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace zuki;

namespace {

struct AssertFired {
    std::string expr;
    std::string message;
};

// A handler that throws instead of aborting, so tests can observe failures.
[[noreturn]] void throwingHandler(const SourceLocation&, std::string_view expr,
                                  std::string_view message) {
    throw AssertFired{std::string{expr}, std::string{message}};
}

// RAII: install the throwing handler for the duration of a test, then restore.
struct HandlerGuard {
    AssertHandler previous;
    HandlerGuard() : previous(setAssertHandler(&throwingHandler)) {}
    ~HandlerGuard() { setAssertHandler(previous); }
};

}  // namespace

TEST_CASE("ZUKI_ENSURE passes silently when the condition holds", "[core][assert]") {
    HandlerGuard guard;
    // The assert macros are statements, so wrap them in a lambda to pass to Catch.
    REQUIRE_NOTHROW([] { ZUKI_ENSURE(1 + 1 == 2); }());
}

TEST_CASE("ZUKI_ENSURE fires the handler when the condition fails", "[core][assert]") {
    HandlerGuard guard;
    REQUIRE_THROWS_AS([] { ZUKI_ENSURE(1 == 2); }(), AssertFired);
}

TEST_CASE("ZUKI_ENSURE_MSG forwards the message", "[core][assert]") {
    HandlerGuard guard;
    try {
        ZUKI_ENSURE_MSG(false, "boom");
        FAIL("expected the assertion to fire");
    } catch (const AssertFired& e) {
        REQUIRE(e.message == "boom");
        REQUIRE(e.expr == "false");
    }
}

TEST_CASE("ZUKI_PANICF formats its message", "[core][assert]") {
    HandlerGuard guard;
    try {
        ZUKI_PANICF("value {} out of range", 7);
        FAIL("expected the panic to fire");
    } catch (const AssertFired& e) {
        REQUIRE(e.message == "value 7 out of range");
    }
}

TEST_CASE("setAssertHandler(nullptr) restores the default", "[core][assert]") {
    const AssertHandler custom = setAssertHandler(&throwingHandler);
    const AssertHandler restored = setAssertHandler(nullptr);
    REQUIRE(restored == &throwingHandler);
    // Put back whatever was there before this test started.
    setAssertHandler(custom);
}
