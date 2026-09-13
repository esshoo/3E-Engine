#include "apps/studio/game_registry.h"
#include "apps/studio/project_registry.h"
#include "apps/studio/studio_app.h"
#include "engine/runtime/runtime_root.h"

#include "p3d/context.h"
#include "pddi/pddi.h"
#include "pddi/pddidev.h"

#include <cstring>
#include <fstream>

namespace {

threee::studio::StudioApp* g_studioApp = nullptr;

void DrawStudioOverlay() {
    if (g_studioApp) {
        g_studioApp->Draw();
    }
}

int VerifyRuntimeData(
    const std::filesystem::path& runtimeRoot) {

    threee::studio::GameRegistry gameRegistry(runtimeRoot);
    gameRegistry.Update(true);

    threee::studio::ProjectRegistry projectRegistry(runtimeRoot);
    projectRegistry.Update(true);

    const std::filesystem::path reportPath =
        runtimeRoot / "3E-Studio-Verify.txt";

    std::ofstream report(reportPath, std::ios::binary);

    if (report) {
        report
            << "3E Studio Runtime Verification\n"
            << "==============================\n\n"
            << "RuntimeRoot=" << runtimeRoot.string() << "\n\n"
            << "GamesDirectory="
            << gameRegistry.GetGamesDirectory().string() << "\n"
            << "GameRegistryError="
            << gameRegistry.GetLastError() << "\n"
            << "ValidGames="
            << gameRegistry.GetGames().size() << "\n"
            << "InvalidGames="
            << gameRegistry.GetInvalidGames().size() << "\n\n";

        for (const auto& game : gameRegistry.GetGames()) {
            report
                << "Game=" << game.id
                << " | " << game.displayName
                << "\n";
        }

        report
            << "\nProjectsDirectory="
            << projectRegistry.GetProjectsDirectory().string() << "\n"
            << "ProjectRegistryError="
            << projectRegistry.GetLastError() << "\n"
            << "ValidProjects="
            << projectRegistry.GetProjects().size() << "\n"
            << "InvalidProjects="
            << projectRegistry.GetInvalidProjects().size() << "\n\n";

        for (const auto& project : projectRegistry.GetProjects()) {
            report
                << "Project=" << project.id
                << " | " << project.displayName
                << " | game=" << project.gameId << "\n"
                << "  GameRoot=" << project.gameRoot.string() << "\n"
                << "  ExportedAssets=" << project.exportedAssets.string() << "\n"
                << "  Overlay=" << project.overlayPath.string() << "\n";
        }

        for (const auto& project : projectRegistry.GetInvalidProjects()) {
            report
                << "InvalidProject="
                << project.manifestPath.string() << "\n"
                << "  Error=" << project.error << "\n";
        }
    }

    if (!gameRegistry.GetLastError().empty()) {
        return 20;
    }

    if (gameRegistry.GetGames().empty()) {
        return 21;
    }

    if (!projectRegistry.GetLastError().empty()) {
        return 30;
    }

    if (projectRegistry.GetProjects().empty()) {
        return 31;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path runtimeRoot =
        threee::runtime::FindRuntimeRoot(
            argv && argv[0] ? argv[0] : nullptr);

    if (argc >= 2 &&
        argv[1] &&
        std::strcmp(argv[1], "--verify-data") == 0) {

        return VerifyRuntimeData(runtimeRoot);
    }

    tPlatform* platform = tPlatform::Create();

    tContextInitData init;
    init.xSize = 1280;
    init.ySize = 800;
    init.title = "3E Studio";
    init.fullscreen = false;
    init.vsync = true;
    init.msaa = 0;

    tContext* context = platform->CreateContext(init);

    if (!context) {
        tPlatform::Destroy();
        return 1;
    }

    threee::studio::StudioApp studio(runtimeRoot);
    g_studioApp = &studio;

    p3d::display->AddOverlayCallback(DrawStudioOverlay);
    p3d::context->SetClearColour(pddiColour(22, 23, 27));

    while (!p3d::display->ShouldClose() && !studio.ShouldExit()) {
        p3d::display->PollEvents();

        studio.Update();

        context->BeginFrame();
        p3d::context->Clear(PDDI_BUFFER_ALL);
        context->EndFrame();

        p3d::display->RenderOverlay();
        context->SwapBuffers();
    }

    g_studioApp = nullptr;

    platform->DestroyContext(context);
    tPlatform::Destroy();

    return 0;
}