// AssetManager — the façade: register importers, then load / cache / hot-reload
// assets by virtual path.
//
//   struct Texture { u32 w, h; std::vector<byte> pixels; };
//
//   AssetManager assets(vfs, &jobs);
//   assets.registerImporter<Texture>({"png", "tga"},
//       [](std::span<const byte> bytes) -> Result<std::shared_ptr<Texture>> {
//           return Ok(std::make_shared<Texture>(decodeImage(bytes)));
//       });
//
//   AssetHandle<Texture> tex = assets.load<Texture>("/textures/hero.png");   // sync
//   AssetHandle<Texture> bg  = assets.loadAsync<Texture>("/textures/bg.png"); // via jobs
//   assets.waitForPending();
//   if (tex.isLoaded()) use(*tex);
//
// Repeated loads of the same path share one record (ref-counted). `reload()`
// re-imports in place and bumps the handle's version(); `garbageCollect()` evicts
// cached assets that no handle still references.
//
// Threading: load/loadAsync/reload/find and async completion are mutex-guarded
// and safe to call concurrently. A T* from get() is valid while a handle to it
// lives and that asset is not concurrently reloaded/unloaded.
#pragma once

#include <zuki/assets/asset.hpp>
#include <zuki/assets/asset_id.hpp>
#include <zuki/core/result.hpp>
#include <zuki/core/types.hpp>

#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace zuki::filesystem {
class FileSystem;
}
namespace zuki::jobs {
class JobSystem;
}

namespace zuki::assets {

class AssetManager {
public:
    // `fs` supplies the bytes (its lifetime must outlive the manager). `jobs` is
    // optional: without it, loadAsync() imports synchronously on the caller.
    explicit AssetManager(filesystem::FileSystem& fs, jobs::JobSystem* jobs = nullptr);
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Register how to turn file bytes into a T, for the given extensions (with or
    // without a leading dot; matched case-insensitively). Re-registering an
    // extension replaces the previous importer.
    template <class T, class Fn>
    void registerImporter(std::initializer_list<std::string_view> extensions, Fn&& importer) {
        Importer imp;
        imp.type = assetType<T>();
        imp.load = [fn = std::forward<Fn>(importer)](
                       std::span<const byte> bytes) -> Result<std::shared_ptr<void>> {
            Result<std::shared_ptr<T>> r = fn(bytes);
            if (r.isErr()) return Err(std::move(r.error()));
            return Ok(std::static_pointer_cast<void>(std::move(r.value())));
        };
        registerImporterErased(extensions, std::move(imp));
    }

    // Load `path`, blocking until the import finishes. Cached: a second call
    // returns the same record.
    template <class T>
    [[nodiscard]] AssetHandle<T> load(std::string_view path) {
        return AssetHandle<T>{loadErased(path, assetType<T>(), /*async=*/false)};
    }

    // Begin loading `path` on the job system; returns immediately in the Loading
    // state (Loaded state observable after completion / waitForPending()).
    template <class T>
    [[nodiscard]] AssetHandle<T> loadAsync(std::string_view path) {
        return AssetHandle<T>{loadErased(path, assetType<T>(), /*async=*/true)};
    }

    // Return the cached handle for `path` without triggering a load (invalid
    // handle if it isn't cached).
    template <class T>
    [[nodiscard]] AssetHandle<T> find(std::string_view path) const {
        return AssetHandle<T>{findErased(assetIdFromPath(normalizePath(path)))};
    }

    // Re-import from the filesystem, swapping content in place and bumping
    // version(). Returns false if the path isn't cached. On import failure the
    // asset goes to Failed but keeps its previously loaded content.
    bool reload(std::string_view path);
    // Reload every cached asset; returns the number successfully reloaded.
    usize reloadAll();

    // Drop `path` from the cache. Live handles keep the record (and its content)
    // alive until they are all released.
    bool unload(std::string_view path);

    // Evict cached records that no external handle references. Returns the count
    // evicted. In-flight async loads are never evicted.
    usize garbageCollect();

    // Block until all async loads have finished (no-op without a job system).
    void waitForPending();

    [[nodiscard]] usize assetCount() const;
    [[nodiscard]] bool isCached(std::string_view path) const;

private:
    // A type-erased importer: bytes -> concrete asset (as shared_ptr<void>).
    struct Importer {
        AssetType type = nullptr;
        std::function<Result<std::shared_ptr<void>>(std::span<const byte>)> load;
    };

    // Implemented in the .cpp so filesystem/jobs stay private to this module.
    void registerImporterErased(std::initializer_list<std::string_view> extensions, Importer imp);
    [[nodiscard]] std::shared_ptr<detail::AssetRecord> loadErased(std::string_view path,
                                                                  AssetType type, bool async);
    [[nodiscard]] std::shared_ptr<detail::AssetRecord> findErased(AssetId id) const;
    void importInto(const std::shared_ptr<detail::AssetRecord>& record);
    [[nodiscard]] static std::string normalizePath(std::string_view path);

    filesystem::FileSystem& fs_;
    jobs::JobSystem* jobs_;

    mutable std::mutex mutex_;
    std::unordered_map<AssetId, std::shared_ptr<detail::AssetRecord>> records_;
    std::unordered_map<std::string, Importer> importersByExt_;  // key: lowercase, no dot
};

}  // namespace zuki::assets
