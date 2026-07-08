// DynamicLibrary — load a shared library at runtime and resolve symbols from it
// (dlopen/dlsym on POSIX, LoadLibrary/GetProcAddress on Windows). The engine
// uses this for plugins and optional backends.
#pragma once

#include <zuki/core/types.hpp>

#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace zuki::platform {

class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    // Load a shared library by path. An empty path resolves symbols from the
    // current process instead. Returns false on failure (see any OS error).
    bool load(std::string_view path);
    void unload() noexcept;
    [[nodiscard]] bool isLoaded() const noexcept { return handle_ != nullptr; }

    // Resolve a symbol by name; nullptr if not found (or nothing loaded).
    [[nodiscard]] void* getSymbol(std::string_view name) const;

    // Typed convenience: resolve a function pointer. `F` must be a function
    // pointer type. Uses memcpy to sidestep the ISO object<->function pointer
    // conversion rule that dlsym-style casts run afoul of.
    template <class F>
    [[nodiscard]] F getFunction(std::string_view name) const {
        static_assert(std::is_pointer_v<F>, "F must be a function pointer type");
        void* symbol = getSymbol(name);
        F fn = nullptr;
        std::memcpy(&fn, &symbol, sizeof(fn));
        return fn;
    }

    // The platform's dynamic library file extension (".so", ".dll", ".dylib").
    [[nodiscard]] static std::string_view nativeExtension() noexcept;

private:
    void* handle_ = nullptr;
};

}  // namespace zuki::platform
