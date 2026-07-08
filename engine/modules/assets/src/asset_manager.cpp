#include <zuki/assets/asset_manager.hpp>

#include <zuki/filesystem/path.hpp>
#include <zuki/filesystem/virtual_file_system.hpp>
#include <zuki/jobs/job_system.hpp>

#include <utility>
#include <vector>

namespace zuki::assets {
namespace {

// Lowercase an extension and drop any leading dot, so ".PNG", "PNG" and "png"
// all key the same importer.
std::string extensionKey(std::string_view ext) {
    if (!ext.empty() && ext.front() == '.') ext.remove_prefix(1);
    std::string key;
    key.reserve(ext.size());
    for (const char c : ext) {
        key.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return key;
}

}  // namespace

AssetManager::AssetManager(filesystem::FileSystem& fs, jobs::JobSystem* jobs)
    : fs_(fs), jobs_(jobs) {}

AssetManager::~AssetManager() {
    // Ensure no async import is still touching a record we're about to destroy.
    waitForPending();
}

std::string AssetManager::normalizePath(std::string_view path) {
    return filesystem::path::normalize(path);
}

void AssetManager::registerImporterErased(std::initializer_list<std::string_view> extensions,
                                          Importer imp) {
    const std::lock_guard lock(mutex_);
    for (const std::string_view ext : extensions) {
        importersByExt_[extensionKey(ext)] = imp;
    }
}

std::shared_ptr<detail::AssetRecord> AssetManager::loadErased(std::string_view path,
                                                             AssetType type, bool async) {
    const std::string norm = normalizePath(path);
    const AssetId id = assetIdFromPath(norm);

    std::shared_ptr<detail::AssetRecord> record;
    {
        const std::lock_guard lock(mutex_);
        if (const auto it = records_.find(id); it != records_.end()) {
            return it->second;  // already cached (loading/loaded/failed)
        }
        record = std::make_shared<detail::AssetRecord>();
        record->id = id;
        record->path = norm;
        record->type = type;
        records_[id] = record;
    }

    if (async && jobs_ != nullptr) {
        record->state.store(AssetState::Loading, std::memory_order_release);
        jobs_->submit([this, record] { importInto(record); });
    } else {
        importInto(record);  // synchronous import sets the final state
    }
    return record;
}

std::shared_ptr<detail::AssetRecord> AssetManager::findErased(AssetId id) const {
    const std::lock_guard lock(mutex_);
    const auto it = records_.find(id);
    return it == records_.end() ? nullptr : it->second;
}

void AssetManager::importInto(const std::shared_ptr<detail::AssetRecord>& record) {
    const auto fail = [&record](std::string message) {
        record->error = std::move(message);
        record->state.store(AssetState::Failed, std::memory_order_release);
    };

    // Find the importer for this file's extension (copied out so we don't hold
    // the lock while reading/decoding).
    const std::string key = extensionKey(filesystem::path::extension(record->path));
    std::function<Result<std::shared_ptr<void>>(std::span<const byte>)> loadFn;
    {
        const std::lock_guard lock(mutex_);
        const auto it = importersByExt_.find(key);
        if (it == importersByExt_.end()) {
            fail("no importer registered for extension '." + key + "'");
            return;
        }
        loadFn = it->second.load;
    }

    Result<std::vector<byte>> bytes = fs_.readFileBinary(record->path);
    if (bytes.isErr()) {
        fail(std::move(bytes.error().message));
        return;
    }

    Result<std::shared_ptr<void>> imported = loadFn(std::span<const byte>{bytes.value()});
    if (imported.isErr()) {
        fail(std::move(imported.error().message));
        return;
    }

    // Publish: content first, then version, then the state that gates readers.
    record->data.store(std::move(imported.value()), std::memory_order_release);
    record->version.fetch_add(1, std::memory_order_release);
    record->state.store(AssetState::Loaded, std::memory_order_release);
}

bool AssetManager::reload(std::string_view path) {
    const AssetId id = assetIdFromPath(normalizePath(path));
    std::shared_ptr<detail::AssetRecord> record;
    {
        const std::lock_guard lock(mutex_);
        const auto it = records_.find(id);
        if (it == records_.end()) return false;
        record = it->second;
    }
    importInto(record);
    return true;
}

usize AssetManager::reloadAll() {
    std::vector<std::shared_ptr<detail::AssetRecord>> snapshot;
    {
        const std::lock_guard lock(mutex_);
        snapshot.reserve(records_.size());
        for (const auto& [id, record] : records_) snapshot.push_back(record);
    }
    usize reloaded = 0;
    for (const auto& record : snapshot) {
        importInto(record);
        if (record->state.load(std::memory_order_acquire) == AssetState::Loaded) ++reloaded;
    }
    return reloaded;
}

bool AssetManager::unload(std::string_view path) {
    const AssetId id = assetIdFromPath(normalizePath(path));
    const std::lock_guard lock(mutex_);
    return records_.erase(id) > 0;
}

usize AssetManager::garbageCollect() {
    const std::lock_guard lock(mutex_);
    usize evicted = 0;
    for (auto it = records_.begin(); it != records_.end();) {
        // use_count 1 == only this map holds the record: no live handles and no
        // in-flight async job captured it.
        if (it->second.use_count() == 1) {
            it = records_.erase(it);
            ++evicted;
        } else {
            ++it;
        }
    }
    return evicted;
}

void AssetManager::waitForPending() {
    if (jobs_ != nullptr) jobs_->waitIdle();
}

usize AssetManager::assetCount() const {
    const std::lock_guard lock(mutex_);
    return records_.size();
}

bool AssetManager::isCached(std::string_view path) const {
    const std::lock_guard lock(mutex_);
    return records_.contains(assetIdFromPath(normalizePath(path)));
}

}  // namespace zuki::assets
