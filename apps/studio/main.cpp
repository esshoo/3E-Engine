#include "apps/studio/game_registry.h"
#include "apps/studio/studio_app.h"

#include "p3d/context.h"
#include "pddi/pddi.h"
#include "pddi/pddidev.h"

#include <cstring>

namespace {

threee::studio::StudioApp* g_studioApp = nullptr;

void DrawStudioOverlay() {
    if (g_studioApp) {
        g_studioApp->Draw();
    }
}

int VerifyRuntimeData(const std::filesystem::path& runtimeRoot) {
    threee::studio::GameRegistry registry(runtimeRoot);
    registry.Update(true);

    if (!registry.GetLastError().empty()) {
        return 2;
    }

    if (registry.GetGames().empty()) {
        return 3;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path runtimeRoot =
        threee::studio::FindRuntimeRoot(argv && argv[0] ? argv[0] : nullptr);

    if (argc >= 2 && argv[1] && std::strcmp(argv[1], "--verify-data") == 0) {
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

    while (!p3d::display->ShouldClose()) {
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