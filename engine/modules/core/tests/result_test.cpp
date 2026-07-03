#include <zukiru/core/result.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace zukiru;

namespace {

Result<i32> parsePositive(i32 v) {
    if (v <= 0) return Err(Error{"must be positive", -1});
    return Ok(v);
}

}  // namespace

TEST_CASE("Result carries a success value", "[core][result]") {
    auto r = parsePositive(42);
    REQUIRE(r.isOk());
    REQUIRE_FALSE(r.isErr());
    REQUIRE(static_cast<bool>(r));
    REQUIRE(r.value() == 42);
}

TEST_CASE("Result carries an error value", "[core][result]") {
    auto r = parsePositive(-5);
    REQUIRE(r.isErr());
    REQUIRE_FALSE(static_cast<bool>(r));
    REQUIRE(r.error().message == "must be positive");
    REQUIRE(r.error().code == -1);
}

TEST_CASE("valueOr returns the fallback on error", "[core][result]") {
    REQUIRE(parsePositive(7).valueOr(0) == 7);
    REQUIRE(parsePositive(-1).valueOr(99) == 99);
}

TEST_CASE("map transforms only the success value", "[core][result]") {
    auto doubled = parsePositive(21).map([](i32 v) { return v * 2; });
    REQUIRE(doubled.isOk());
    REQUIRE(doubled.value() == 42);

    auto stillErr = parsePositive(-1).map([](i32 v) { return v * 2; });
    REQUIRE(stillErr.isErr());
    REQUIRE(stillErr.error().code == -1);
}

TEST_CASE("mapErr transforms only the error value", "[core][result]") {
    auto mapped = parsePositive(-1).mapErr([](const Error& e) { return e.code; });
    REQUIRE(mapped.isErr());
    REQUIRE(mapped.error() == -1);

    auto ok = parsePositive(3).mapErr([](const Error& e) { return e.code; });
    REQUIRE(ok.isOk());
    REQUIRE(ok.value() == 3);
}

TEST_CASE("map can change the value type", "[core][result]") {
    auto asString = parsePositive(5).map([](i32 v) { return std::to_string(v); });
    REQUIRE(asString.isOk());
    REQUIRE(asString.value() == "5");
}

TEST_CASE("Status represents payload-free success or failure", "[core][result]") {
    Status ok = Ok();
    REQUIRE(ok.isOk());

    Status bad = Err(Error{"nope"});
    REQUIRE(bad.isErr());
    REQUIRE(bad.error().message == "nope");
}

TEST_CASE("Result move-returns its value", "[core][result]") {
    Result<std::string> r = Ok(std::string{"hello"});
    std::string moved = std::move(r).value();
    REQUIRE(moved == "hello");
}
