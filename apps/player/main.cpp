#include "apps/player/player_app.h"
#include "engine/runtime/runtime_root.h"

#include "p3d/context.h"
#include "pddi/pddi.h"
#include "pddi/pddidev.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace {

threee::player::PlayerApp* g_playerApp =
    nullptr;

void DrawPlayerOverlay() {
    if (g_playerApp) {
        g_playerApp->Draw();
    }
}

bool ParseArguments(
    int argc,
    char** argv,
    threee::player::PlayerLaunchRequest& request,
    bool& verifyRuntime,
    std::string& error) {

    verifyRuntime = false;

    for (int i = 1;
         i < argc;
         ++i) {

        const char* raw =
            argv[i];

        if (!raw) {
            continue;
        }

        const std::string argument(raw);

        if (argument == "--verify-runtime") {
            verifyRuntime = true;
            continue;
        }

        if (argument == "--game") {
            if (i + 1 >= argc ||
                !argv[i + 1]) {

                error =
                    "--game requires a value.";

                return false;
            }

            request.gameId =
                argv[++i];

            continue;
        }

        if (argument == "--project") {
            if (i + 1 >= argc ||
                !argv[i + 1]) {

                error =
                    "--project requires a path.";

                return false;
            }

            request.projectManifest =
                std::filesystem::path(
                    argv[++i]);

            continue;
        }

        if (argument == "--help" ||
            argument == "-h") {

            error =
                "Usage: 3E-Player [--game <id>] [--project <project.3e.json>] [--verify-runtime]";

            return false;
        }

        error =
            "Unknown argument: " +
            argument;

        return false;
    }

    return true;
}

} // namespace

int main(
    int argc,
    char** argv) {

    const std::filesystem::path runtimeRoot =
        threee::runtime::FindRuntimeRoot(
            argv && argv[0]
                ? argv[0]
                : nullptr);

    threee::player::PlayerLaunchRequest request;
    bool verifyRuntime = false;
    std::string argumentError;

    const bool argumentsOk =
        ParseArguments(
            argc,
            argv,
            request,
            verifyRuntime,
            argumentError);

    threee::player::PlayerRuntimeState state;

    if (argumentsOk) {
        state.Load(
            runtimeRoot,
            request);
    }
    else {
        state.runtimeRoot =
            runtimeRoot;

        state.error =
            argumentError;
    }

    if (verifyRuntime) {
        const std::filesystem::path reportPath =
            runtimeRoot /
            "3E-Player-Verify.txt";

        threee::player::WriteVerificationReport(
            state,
            reportPath);

        return
            state.valid
                ? 0
                : 2;
    }

    tPlatform* platform =
        tPlatform::Create();

    tContextInitData init;
    init.xSize = 1280;
    init.ySize = 800;
    init.title = "3E Player";
    init.fullscreen = false;
    init.vsync = true;
    init.msaa = 0;

    tContext* context =
        platform->CreateContext(init);

    if (!context) {
        tPlatform::Destroy();
        return 1;
    }

    threee::player::PlayerApp player(
        std::move(state));

    g_playerApp = &player;

    p3d::display->AddOverlayCallback(
        DrawPlayerOverlay);

    p3d::context->SetClearColour(
        pddiColour(
            14,
            15,
            18));

    while (!p3d::display->ShouldClose()) {
        p3d::display->PollEvents();

        player.Update();

        context->BeginFrame();

        p3d::context->Clear(
            PDDI_BUFFER_ALL);

        context->EndFrame();

        p3d::display->RenderOverlay();
        context->SwapBuffers();
    }

    g_playerApp = nullptr;

    platform->DestroyContext(context);
    tPlatform::Destroy();

    return 0;
}