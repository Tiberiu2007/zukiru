// Asset state, the type-erased record the manager owns, and the typed handle
// game code holds.
//
// An AssetHandle<T> is a ref-counted weak-ish reference: it shares ownership of
// the record (so the record — and any handle-derived pointer — stays alive), but
// the *content* is what actually loads. Query state()/isLoaded() and call get()
// for the T*.
#pragma once

#include <zukiru/assets/asset_id.hpp>
#include <zukiru/core/types.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace zukiru::assets {

// A RTTI-free per-type identity (address of a per-instantiation static), so
// assets works under -fno-rtti and can key importers/records by type.
using AssetType = const void*;
template <class T>
[[nodiscard]] AssetType assetType() noexcept {
    static const char marker{};
    return &marker;
}

enum class AssetState : u8 {
    Unloaded,  // record exists, no import attempted yet
    Loading,   // an async import is in flight
    Loaded,    // content is available via get()
    Failed,    // the last import attempt failed (see error())
};

namespace detail {

// The manager-owned record for one asset. Fields set at creation (id/path/type)
// are immutable; the rest are published across threads via `state` release/acquire.
struct AssetRecord {
    AssetId id;
    std::string path;
    AssetType type = nullptr;

    std::atomic<AssetState> state{AssetState::Unloaded};
    std::atomic<u64> version{0};                 // bumped on every successful (re)load
    std::atomic<std::shared_ptr<void>> data{nullptr};  // the concrete asset
    std::string error;  // meaningful once state == Failed (published via state)
};

}  // namespace detail

// A typed, ref-counted reference to an asset. Cheap to copy. `get()` returns the
// best available content (nullptr until first successful load); the pointer stays
// valid while this handle lives and the asset is not reloaded/unloaded.
template <class T>
class AssetHandle {
public:
    AssetHandle() = default;

    [[nodiscard]] bool valid() const noexcept { return record_ != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] AssetId id() const noexcept { return record_ ? record_->id : AssetId{}; }
    [[nodiscard]] std::string_view path() const noexcept {
        return record_ ? std::string_view{record_->path} : std::string_view{};
    }

    [[nodiscard]] AssetState state() const noexcept {
        return record_ ? record_->state.load(std::memory_order_acquire) : AssetState::Unloaded;
    }
    [[nodiscard]] bool isLoaded() const noexcept { return state() == AssetState::Loaded; }
    [[nodiscard]] bool isFailed() const noexcept { return state() == AssetState::Failed; }

    // Monotonic counter that increments on each successful load/reload — compare
    // against a cached value to notice hot-reloads.
    [[nodiscard]] u64 version() const noexcept {
        return record_ ? record_->version.load(std::memory_order_acquire) : 0;
    }
    // Empty unless the last import failed.
    [[nodiscard]] std::string_view error() const noexcept {
        if (record_ && record_->state.load(std::memory_order_acquire) == AssetState::Failed) {
            return record_->error;
        }
        return {};
    }

    // The loaded content, or nullptr if nothing has loaded yet.
    [[nodiscard]] T* get() const noexcept {
        if (!record_) return nullptr;
        const std::shared_ptr<void> d = record_->data.load(std::memory_order_acquire);
        return static_cast<T*>(d.get());
    }
    [[nodiscard]] T* operator->() const noexcept { return get(); }
    [[nodiscard]] T& operator*() const noexcept { return *get(); }

    // How many references exist to the underlying record (manager + all handles).
    [[nodiscard]] long useCount() const noexcept { return record_ ? record_.use_count() : 0; }

private:
    friend class AssetManager;
    explicit AssetHandle(std::shared_ptr<detail::AssetRecord> record) noexcept
        : record_(std::move(record)) {}

    std::shared_ptr<detail::AssetRecord> record_;
};

}  // namespace zukiru::assets
