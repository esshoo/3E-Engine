#include "apps/studio/studio_app.h"

#include "p3d/context.h"
#include "pddi/pddi.h"
#include "pddi/pddidev.h"

#include <iostream>

namespace {

threee::studio::StudioApp* g_studioApp = nullptr;

void DrawStudioOverlay() {
    if (g_studioApp) {
        g_studioApp->Draw();
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;

    const std::filesystem::path projectRoot =
        threee::studio::FindProjectRoot(argv && argv[0] ? argv[0] : nullptr);

    std::cout << "[3E-Studio] Project root: "
              << projectRoot.string()
              << std::endl;

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
        std::cerr << "[3E-Studio] CreateContext failed." << std::endl;
        tPlatform::Destroy();
        return 1;
    }

    threee::studio::StudioApp studio(projectRoot);
    g_studioApp = &studio;

    p3d::display->AddOverlayCallback(DrawStudioOverlay);
    p3d::context->SetClearColour(pddiColour(22, 23, 27));

    while (!p3d::display->ShouldClose()) {
        // Same lifecycle order used by the existing ReChan runtime:
        // events -> frame -> overlay -> present.
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