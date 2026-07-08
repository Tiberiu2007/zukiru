#include <zuki/platform/dynamic_library.hpp>

#include <string>

#if defined(ZUKI_OS_WINDOWS)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace zuki::platform {

DynamicLibrary::~DynamicLibrary() {
    unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        unload();
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

bool DynamicLibrary::load(std::string_view path) {
    unload();
    const std::string owned{path};
#if defined(ZUKI_OS_WINDOWS)
    handle_ = owned.empty() ? static_cast<void*>(GetModuleHandleA(nullptr))
                            : static_cast<void*>(LoadLibraryA(owned.c_str()));
#else
    handle_ = dlopen(owned.empty() ? nullptr : owned.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    return handle_ != nullptr;
}

void DynamicLibrary::unload() noexcept {
    if (handle_ == nullptr) return;
#if defined(ZUKI_OS_WINDOWS)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
}

void* DynamicLibrary::getSymbol(std::string_view name) const {
    if (handle_ == nullptr) return nullptr;
    const std::string owned{name};
#if defined(ZUKI_OS_WINDOWS)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), owned.c_str()));
#else
    return dlsym(handle_, owned.c_str());
#endif
}

std::string_view DynamicLibrary::nativeExtension() noexcept {
#if defined(ZUKI_OS_WINDOWS)
    return ".dll";
#elif defined(ZUKI_OS_MACOS)
    return ".dylib";
#else
    return ".so";
#endif
}

}  // namespace zuki::platform
