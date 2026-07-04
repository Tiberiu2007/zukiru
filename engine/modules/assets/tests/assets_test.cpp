#include <zukiru/assets/assets.hpp>

#include <zukiru/filesystem/virtual_file_system.hpp>
#include <zukiru/jobs/job_system.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <random>
#include <span>
#include <string>

using namespace zukiru;
using namespace zukiru::assets;
namespace stdfs = std::filesystem;

namespace {

// A parsed text asset — stands in for a "real" cooked resource.
struct TextAsset {
    std::string content;
};

// A unique temp dir with a writable VFS mounted at "/assets".
struct Fixture {
    stdfs::path dir;
    filesystem::FileSystem vfs;

    Fixture() {
        static std::atomic<u64> counter{0};
        std::random_device rd;
        dir = stdfs::temp_directory_path() /
              ("zukiru_assets_" + std::to_string(rd()) + "_" + std::to_string(counter.fetch_add(1)));
        stdfs::create_directories(dir);
        REQUIRE(vfs.mount("/assets", dir, /*writable=*/true));
    }
    ~Fixture() {
        std::error_code ec;
        stdfs::remove_all(dir, ec);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    void write(std::string_view virtualPath, std::string_view contents) {
        REQUIRE(vfs.writeFile(virtualPath, contents).isOk());
    }
};

// Parse bytes into a TextAsset; content beginning with '!' is rejected (models a
// decode failure, used to exercise failure and reload-failure paths).
Result<std::shared_ptr<TextAsset>> importText(std::span<const byte> bytes) {
    std::string s(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!s.empty() && s.front() == '!') return Err(Error{"rejected content"});
    return Ok(std::make_shared<TextAsset>(TextAsset{std::move(s)}));
}

}  // namespace

TEST_CASE("sync load imports, exposes content, and caches", "[assets]") {
    Fixture fx;
    fx.write("/assets/hello.txt", "hi there");

    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/hello.txt");
    REQUIRE(h.valid());
    REQUIRE(h.isLoaded());
    REQUIRE(h.get() != nullptr);
    REQUIRE(h->content == "hi there");
    REQUIRE(h.version() == 1);
    REQUIRE(assets.assetCount() == 1);
    REQUIRE(assets.isCached("/assets/hello.txt"));

    // A second load hits the cache: same record (id + shared ownership).
    AssetHandle<TextAsset> h2 = assets.load<TextAsset>("/assets/hello.txt");
    REQUIRE(h2.id() == h.id());
    REQUIRE(h2.get() == h.get());
    REQUIRE(assets.assetCount() == 1);
}

TEST_CASE("path normalization makes equivalent paths share a record", "[assets]") {
    Fixture fx;
    fx.write("/assets/a.txt", "x");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> a = assets.load<TextAsset>("/assets/a.txt");
    AssetHandle<TextAsset> b = assets.load<TextAsset>("/assets//./a.txt");
    REQUIRE(a.id() == b.id());
    REQUIRE(assets.assetCount() == 1);
}

TEST_CASE("missing importer fails cleanly", "[assets]") {
    Fixture fx;
    fx.write("/assets/data.bin", "raw");
    AssetManager assets(fx.vfs);  // no importer for ".bin"

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/data.bin");
    REQUIRE(h.isFailed());
    REQUIRE(h.get() == nullptr);
    REQUIRE(h.error().find("no importer") != std::string_view::npos);
}

TEST_CASE("missing file fails with the filesystem error", "[assets]") {
    Fixture fx;
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/nope.txt");
    REQUIRE(h.isFailed());
    REQUIRE_FALSE(h.error().empty());
}

TEST_CASE("async load resolves through the job system", "[assets]") {
    Fixture fx;
    for (int i = 0; i < 16; ++i) {
        fx.write("/assets/n" + std::to_string(i) + ".txt", "payload" + std::to_string(i));
    }

    jobs::JobSystem pool(4);
    AssetManager assets(fx.vfs, &pool);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    std::vector<AssetHandle<TextAsset>> handles;
    for (usize i = 0; i < 16; ++i) {
        handles.push_back(assets.loadAsync<TextAsset>("/assets/n" + std::to_string(i) + ".txt"));
    }
    assets.waitForPending();

    for (usize i = 0; i < 16; ++i) {
        REQUIRE(handles[i].isLoaded());
        REQUIRE(handles[i]->content == "payload" + std::to_string(i));
    }
}

TEST_CASE("hot reload swaps content in place and bumps version", "[assets]") {
    Fixture fx;
    fx.write("/assets/cfg.txt", "v1");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/cfg.txt");
    REQUIRE(h->content == "v1");
    const u64 firstVersion = h.version();

    fx.write("/assets/cfg.txt", "v2-edited");
    REQUIRE(assets.reload("/assets/cfg.txt"));

    // Same handle now sees the new content and a newer version.
    REQUIRE(h->content == "v2-edited");
    REQUIRE(h.version() > firstVersion);
}

TEST_CASE("failed reload keeps the last good content", "[assets]") {
    Fixture fx;
    fx.write("/assets/cfg.txt", "good");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/cfg.txt");
    const u64 goodVersion = h.version();

    fx.write("/assets/cfg.txt", "!broken");  // importer rejects this
    REQUIRE(assets.reload("/assets/cfg.txt"));

    REQUIRE(h.isFailed());
    REQUIRE_FALSE(h.error().empty());
    REQUIRE(h->content == "good");        // previous content preserved
    REQUIRE(h.version() == goodVersion);  // no successful reload -> version steady
}

TEST_CASE("reloadAll re-imports every cached asset", "[assets]") {
    Fixture fx;
    fx.write("/assets/a.txt", "a1");
    fx.write("/assets/b.txt", "b1");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> a = assets.load<TextAsset>("/assets/a.txt");
    AssetHandle<TextAsset> b = assets.load<TextAsset>("/assets/b.txt");

    fx.write("/assets/a.txt", "a2");
    fx.write("/assets/b.txt", "b2");
    REQUIRE(assets.reloadAll() == 2);
    REQUIRE(a->content == "a2");
    REQUIRE(b->content == "b2");
}

TEST_CASE("unload drops the cache entry but live handles keep content", "[assets]") {
    Fixture fx;
    fx.write("/assets/keep.txt", "payload");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/keep.txt");
    REQUIRE(assets.unload("/assets/keep.txt"));
    REQUIRE_FALSE(assets.isCached("/assets/keep.txt"));
    REQUIRE(assets.assetCount() == 0);

    // The handle still owns the record, so its content is intact.
    REQUIRE(h->content == "payload");

    // Loading again creates a fresh, independent record.
    AssetHandle<TextAsset> h2 = assets.load<TextAsset>("/assets/keep.txt");
    REQUIRE(assets.assetCount() == 1);
    REQUIRE(h2.get() != h.get());
}

TEST_CASE("garbageCollect evicts only unreferenced assets", "[assets]") {
    Fixture fx;
    fx.write("/assets/held.txt", "1");
    fx.write("/assets/dropped.txt", "2");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    AssetHandle<TextAsset> held = assets.load<TextAsset>("/assets/held.txt");
    { [[maybe_unused]] AssetHandle<TextAsset> temp = assets.load<TextAsset>("/assets/dropped.txt"); }

    REQUIRE(assets.assetCount() == 2);
    REQUIRE(assets.garbageCollect() == 1);  // only "dropped" had no live handle
    REQUIRE(assets.assetCount() == 1);
    REQUIRE(assets.isCached("/assets/held.txt"));
    REQUIRE(held->content == "1");
}

TEST_CASE("find returns cached assets only, never triggering a load", "[assets]") {
    Fixture fx;
    fx.write("/assets/x.txt", "data");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({"txt"}, importText);

    REQUIRE_FALSE(assets.find<TextAsset>("/assets/x.txt").valid());
    REQUIRE(assets.assetCount() == 0);

    (void)assets.load<TextAsset>("/assets/x.txt");
    AssetHandle<TextAsset> found = assets.find<TextAsset>("/assets/x.txt");
    REQUIRE(found.valid());
    REQUIRE(found->content == "data");
}

TEST_CASE("case-insensitive extension matching", "[assets]") {
    Fixture fx;
    fx.write("/assets/UP.TXT", "yo");
    AssetManager assets(fx.vfs);
    assets.registerImporter<TextAsset>({".TXT"}, importText);  // dotted + upper

    AssetHandle<TextAsset> h = assets.load<TextAsset>("/assets/UP.TXT");
    REQUIRE(h.isLoaded());
    REQUIRE(h->content == "yo");
}
