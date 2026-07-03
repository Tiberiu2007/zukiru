// Result<T, E> — an explicit success-or-error value, for error handling without
// exceptions. Modeled on Rust's Result / the std::expected shape (which is
// C++23 and not yet available on our baseline).
//
//   Result<Texture> loadTexture(std::string_view path) {
//       if (!exists(path)) return Err(Error{"not found"});
//       return Ok(decode(path));
//   }
//
//   auto r = loadTexture("a.png");
//   if (r) use(r.value());
//   else   log(r.error().message);
//
// `Status` is the payload-free variant (success carries `Unit`).
#pragma once

#include <zukiru/core/assert.hpp>
#include <zukiru/core/types.hpp>

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace zuki {

// Default error type: a human-readable message plus an optional integer code.
struct Error {
    std::string message;
    i32 code = 0;

    Error() = default;
    explicit Error(std::string msg, i32 c = 0) : message(std::move(msg)), code(c) {}
};

// --- Ok / Err construction tags ------------------------------------------
namespace detail {

template <class T>
struct OkWrap {
    T value;
};
template <>
struct OkWrap<void> {};

template <class E>
struct ErrWrap {
    E error;
};

}  // namespace detail

// Wrap a success value. `Ok()` (no args) builds a success for a void/Status result.
template <class T>
[[nodiscard]] constexpr auto Ok(T&& value) {
    return detail::OkWrap<std::decay_t<T>>{std::forward<T>(value)};
}
[[nodiscard]] inline constexpr detail::OkWrap<void> Ok() {
    return {};
}

// Wrap an error value.
template <class E>
[[nodiscard]] constexpr auto Err(E&& error) {
    return detail::ErrWrap<std::decay_t<E>>{std::forward<E>(error)};
}

// -------------------------------------------------------------------------
template <class T, class E = Error>
class [[nodiscard]] Result {
    static_assert(!std::is_void_v<T>, "use Result<void, E> / Status for no payload");

public:
    using ValueType = T;
    using ErrorType = E;

    // Implicit construction from Ok(...) / Err(...) so callers can `return Ok(x);`.
    template <class U>
    constexpr Result(detail::OkWrap<U>&& ok)  // NOLINT(google-explicit-constructor)
        : storage_(std::in_place_index<0>, std::move(ok.value)) {}
    template <class G>
    constexpr Result(detail::ErrWrap<G>&& err)  // NOLINT(google-explicit-constructor)
        : storage_(std::in_place_index<1>, std::move(err.error)) {}

    [[nodiscard]] constexpr bool isOk() const noexcept { return storage_.index() == 0; }
    [[nodiscard]] constexpr bool isErr() const noexcept { return storage_.index() == 1; }
    constexpr explicit operator bool() const noexcept { return isOk(); }

    // Access the success value. Precondition: isOk().
    constexpr T& value() & {
        ZUKI_ENSURE_MSG(isOk(), "Result::value() called on an error result");
        return std::get<0>(storage_);
    }
    constexpr const T& value() const& {
        ZUKI_ENSURE_MSG(isOk(), "Result::value() called on an error result");
        return std::get<0>(storage_);
    }
    constexpr T&& value() && {
        ZUKI_ENSURE_MSG(isOk(), "Result::value() called on an error result");
        return std::get<0>(std::move(storage_));
    }

    // Access the error. Precondition: isErr().
    constexpr E& error() & {
        ZUKI_ENSURE_MSG(isErr(), "Result::error() called on an ok result");
        return std::get<1>(storage_);
    }
    constexpr const E& error() const& {
        ZUKI_ENSURE_MSG(isErr(), "Result::error() called on an ok result");
        return std::get<1>(storage_);
    }

    // Return the value, or `fallback` if this is an error.
    template <class U>
    [[nodiscard]] constexpr T valueOr(U&& fallback) const& {
        return isOk() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
    }
    template <class U>
    [[nodiscard]] constexpr T valueOr(U&& fallback) && {
        return isOk() ? std::get<0>(std::move(storage_))
                      : static_cast<T>(std::forward<U>(fallback));
    }

    // Transform the success value, preserving any error. `f: T -> U`.
    template <class F>
    [[nodiscard]] constexpr auto map(F&& f) const& -> Result<std::decay_t<decltype(f(value()))>, E> {
        using R = Result<std::decay_t<decltype(f(std::get<0>(storage_)))>, E>;
        if (isOk()) return R(Ok(f(std::get<0>(storage_))));
        return R(Err(std::get<1>(storage_)));
    }

    // Transform the error value, preserving any success. `f: E -> G`.
    template <class F>
    [[nodiscard]] constexpr auto mapErr(F&& f) const&
        -> Result<T, std::decay_t<decltype(f(error()))>> {
        using R = Result<T, std::decay_t<decltype(f(std::get<1>(storage_)))>>;
        if (isErr()) return R(Err(f(std::get<1>(storage_))));
        return R(Ok(std::get<0>(storage_)));
    }

private:
    std::variant<T, E> storage_;
};

// --- Result<void, E> / Status --------------------------------------------
template <class E>
class [[nodiscard]] Result<void, E> {
public:
    using ValueType = void;
    using ErrorType = E;

    constexpr Result(detail::OkWrap<void>) {}  // NOLINT(google-explicit-constructor)
    template <class G>
    constexpr Result(detail::ErrWrap<G>&& err)  // NOLINT(google-explicit-constructor)
        : error_(std::move(err.error)), hasError_(true) {}

    [[nodiscard]] constexpr bool isOk() const noexcept { return !hasError_; }
    [[nodiscard]] constexpr bool isErr() const noexcept { return hasError_; }
    constexpr explicit operator bool() const noexcept { return isOk(); }

    constexpr E& error() & {
        ZUKI_ENSURE_MSG(isErr(), "Result::error() called on an ok result");
        return error_;
    }
    constexpr const E& error() const& {
        ZUKI_ENSURE_MSG(isErr(), "Result::error() called on an ok result");
        return error_;
    }

private:
    E error_{};
    bool hasError_ = false;
};

// A result that carries no payload on success — just success or an error.
using Status = Result<void, Error>;

}  // namespace zuki
