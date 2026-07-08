#include <zuki/app/app.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki;
using namespace zuki::app;

TEST_CASE("AppConfig has sensible defaults", "[app]") {
    const AppConfig cfg;
    REQUIRE(cfg.window.title == "Zuki");
    REQUIRE(cfg.device.backend == render::Backend::Vulkan);
    REQUIRE(cfg.clearColor.a == 1.0f);
}

namespace {

// A game that runs a fixed number of frames and then quits, recording that the
// lifecycle hooks fired in order.
struct FrameLimitGame : Application {
    u64 limit;
    bool started = false;
    bool shutdown = false;
    u64 updates = 0;
    u64 renders = 0;

    explicit FrameLimitGame(u64 frames) : limit(frames) {}

    void onStart(App& app) override {
        started = true;
        app.device().setClearColor({0.2f, 0.3f, 0.5f, 1.0f});
    }
    void onUpdate(App& app, f32 /*dt*/) override {
        ++updates;
        if (app.frameCount() >= limit) app.requestQuit();
    }
    void onRender(App& /*app*/) override { ++renders; }
    void onShutdown(App& /*app*/) override { shutdown = true; }
};

}  // namespace

// Needs a display + GPU, so hidden by default. Run with:
//   zuki_app_tests "[.app]"
TEST_CASE("App runs the main loop and dispatches lifecycle hooks", "[.app]") {
    Result<std::unique_ptr<App>> appResult = App::create({.window = {.title = "Zuki app test"}});
    if (appResult.isErr()) {
        FAIL(appResult.error().message);
    }
    std::unique_ptr<App>& app = appResult.value();

    FrameLimitGame game(10);
    app->run(game);

    REQUIRE(game.started);
    REQUIRE(game.shutdown);
    REQUIRE(app->frameCount() >= 10);
    REQUIRE(game.updates >= 10);
    REQUIRE(game.renders >= 10);  // onRender fired inside the render pass
}
