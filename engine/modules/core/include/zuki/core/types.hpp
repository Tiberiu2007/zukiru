// Core vocabulary types for Zuki. These live in the root `zuki` namespace on
// purpose (see docs/adr/0002-core-root-namespace.md) because they appear in the
// signatures of nearly every other module.
#pragma once

#include <cstddef>
#include <cstdint>

namespace zuki {

// --- Fixed-width integer aliases -----------------------------------------
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// --- Floating point ------------------------------------------------------
using f32 = float;
using f64 = double;

// --- Size / pointer-sized ------------------------------------------------
using usize = std::size_t;     // unsigned, index/size type
using isize = std::ptrdiff_t;  // signed, pointer difference
using uptr = std::uintptr_t;   // integer wide enough to hold a pointer
using iptr = std::intptr_t;

// Raw byte. Prefer this over `char`/`unsigned char` for opaque memory.
using byte = std::byte;

// --- Unit --------------------------------------------------------------
// An empty "no value" type. Used e.g. as the success type of a Result that
// carries no payload (`Status`), where `void` would be awkward.
struct Unit {};

inline constexpr Unit kUnit{};

// --- Static size guarantees ----------------------------------------------
static_assert(sizeof(i8) == 1 && sizeof(u8) == 1);
static_assert(sizeof(i16) == 2 && sizeof(u16) == 2);
static_assert(sizeof(i32) == 4 && sizeof(u32) == 4);
static_assert(sizeof(i64) == 8 && sizeof(u64) == 8);
static_assert(sizeof(f32) == 4 && sizeof(f64) == 8);
static_assert(sizeof(uptr) == sizeof(void*));

// --- Integer literal helpers ---------------------------------------------
// Convenience user-defined literals for the common unsigned sizes, e.g.
// `for (usize i = 0; i < 10_uz; ++i)`.
inline namespace literals {

consteval usize operator""_uz(unsigned long long v) {
    return static_cast<usize>(v);
}
consteval u32 operator""_u32(unsigned long long v) {
    return static_cast<u32>(v);
}
consteval u64 operator""_u64(unsigned long long v) {
    return static_cast<u64>(v);
}

}  // namespace literals

}  // namespace zuki
