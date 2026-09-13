#include "apps/player/player_app.h"

#include "engine/data/json_value.h"

#include <imgui.h>

#include <fstream>
#include <system_error>
#include <utility>

namespace threee::player {

namespace {

std::filesystem::path ResolveProjectPath(
    const std::filesystem::path& projectRoot,
    const std::string& value) {

    if (value.empty()) {
        return {};
    }

    std::filesystem::path path(value);

    if (path.is_absolute()) {
        return
            path.lexically_normal();
    }

    return
        (projectRoot / path)
            .lexically_normal();
}

std::string NestedString(
    const data::JsonValue& root,
    const char* objectName,
    const char* key) {

    const data::JsonValue* object =
        root.Find(objectName);

    if (!object ||
        !object->IsObject()) {
        return {};
    }

    return
        object->GetString(key);
}

bool Exists(
    const std::filesystem::path& path) {

    if (path.empty()) {
        return false;
    }

    std::error_code ec;

    return
        std::filesystem::exists(
            path,
            ec);
}

} // namespace

bool PlayerRuntimeState::Load(
    std::filesystem::path inRuntimeRoot,
    const PlayerLaunchRequest& request) {

    *this = {};

    runtimeRoot =
        std::move(inRuntimeRoot);

    gameId =
        request.gameId;

    if (!request.projectManifest.empty()) {
        projectManifest =
            request.projectManifest;

        if (projectManifest.is_relative()) {
            std::error_code ec;

            projectManifest =
                std::filesystem::absolute(
                    projectManifest,
                    ec);

            if (ec) {
                error =
                    "Cannot resolve project path: " +
                    ec.message();

                return false;
            }
        }

        projectManifest =
            projectManifest.lexically_normal();

        if (!Exists(projectManifest)) {
            error =
                "Project manifest not found: " +
                projectManifest.string();

            return false;
        }

        const data::JsonDocument project =
            data::JsonDocument::LoadFile(
                projectManifest);

        if (!project.Ok()) {
            error =
                "Cannot parse project manifest: " +
                project.error;

            return false;
        }

        if (!project.root.IsObject()) {
            error =
                "Project manifest root must be an object.";

            return false;
        }

        projectRoot =
            projectManifest.parent_path();

        projectId =
            project.root.GetString("id");

        projectDisplayName =
            project.root.GetString(
                "displayName",
                projectId);

        const std::string projectGame =
            project.root.GetString("game");

        if (gameId.empty()) {
            gameId = projectGame;
        }
        else if (!projectGame.empty() &&
                 projectGame != gameId) {

            error =
                "Project game mismatch. Requested '" +
                gameId +
                "' but project uses '" +
                projectGame +
                "'.";

            return false;
        }

        sourceGameRoot =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "source",
                    "gameRoot"));

        exportedAssets =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "source",
                    "exportedAssets"));

        overlayPath =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "workspace",
                    "overlay"));

        if (!sourceGameRoot.empty() &&
            !Exists(sourceGameRoot)) {

            error =
                "Project GameRoot not found: " +
                sourceGameRoot.string();

            return false;
        }

        if (!exportedAssets.empty() &&
            !Exists(exportedAssets)) {

            error =
                "Project ExportedAssets not found: " +
                exportedAssets.string();

            return false;
        }
    }

    if (gameId.empty()) {
        error =
            "No game was selected. Use --game <id> or --project <manifest>.";

        return false;
    }

    gameManifest =
        runtimeRoot /
        "games" /
        gameId /
        "game.json";

    if (!Exists(gameManifest)) {
        error =
            "Game manifest not found: " +
            gameManifest.string();

        return false;
    }

    const data::JsonDocument game =
        data::JsonDocument::LoadFile(
            gameManifest);

    if (!game.Ok()) {
        error =
            "Cannot parse game manifest: " +
            game.error;

        return false;
    }

    if (!game.root.IsObject()) {
        error =
            "Game manifest root must be an object.";

        return false;
    }

    const std::string manifestGameId =
        game.root.GetString("id");

    if (manifestGameId.empty()) {
        error =
            "Game manifest is missing id.";

        return false;
    }

    if (manifestGameId != gameId) {
        error =
            "Game manifest id mismatch. Folder requested '" +
            gameId +
            "', manifest declares '" +
            manifestGameId +
            "'.";

        return false;
    }

    if (!game.root.GetBool(
            "enabled",
            true)) {

        error =
            "Game is disabled: " +
            gameId;

        return false;
    }

    gameDisplayName =
        game.root.GetString(
            "displayName",
            gameId);

    runtimeHost =
        NestedString(
            game.root,
            "runtime",
            "host");

    runtimeMode =
        NestedString(
            game.root,
            "runtime",
            "mode");

    if (!runtimeHost.empty() &&
        runtimeHost != "3E-Player") {

        error =
            "Game runtime host is not 3E-Player: " +
            runtimeHost;

        return false;
    }

    valid = true;
    error.clear();

    return true;
}

PlayerApp::PlayerApp(
    PlayerRuntimeState state)
    : m_state(std::move(state)) {
}

void PlayerApp::Update() {
}

void PlayerApp::Draw() {
    ImGui::SetNextWindowSize(
        ImVec2(760.0f, 500.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(
            "3E Player - Runtime")) {

        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(
        "3E Player");

    ImGui::Separator();

    ImGui::TextWrapped(
        "Runtime root: %s",
        m_state.runtimeRoot
            .string()
            .c_str());

    ImGui::Separator();

    if (!m_state.valid) {
        ImGui::TextUnformatted(
            "Runtime launch failed.");

        ImGui::TextWrapped(
            "Error: %s",
            m_state.error.c_str());

        ImGui::End();
        return;
    }

    ImGui::Text(
        "Game: %s [%s]",
        m_state.gameDisplayName.c_str(),
        m_state.gameId.c_str());

    ImGui::TextWrapped(
        "Game manifest: %s",
        m_state.gameManifest
            .string()
            .c_str());

    ImGui::Text(
        "Runtime host: %s",
        m_state.runtimeHost.empty()
            ? "unspecified"
            : m_state.runtimeHost.c_str());

    ImGui::Text(
        "Runtime mode: %s",
        m_state.runtimeMode.empty()
            ? "unspecified"
            : m_state.runtimeMode.c_str());

    ImGui::Separator();

    if (!m_state.projectManifest.empty()) {
        ImGui::Text(
            "Project: %s [%s]",
            m_state.projectDisplayName.c_str(),
            m_state.projectId.c_str());

        ImGui::TextWrapped(
            "Project manifest: %s",
            m_state.projectManifest
                .string()
                .c_str());

        ImGui::TextWrapped(
            "Game source: %s",
            m_state.sourceGameRoot
                .string()
                .c_str());

        ImGui::TextWrapped(
            "ExportedAssets: %s",
            m_state.exportedAssets
                .string()
                .c_str());

        ImGui::TextWrapped(
            "Overlay: %s",
            m_state.overlayPath
                .string()
                .c_str());
    }
    else {
        ImGui::TextDisabled(
            "No project manifest was supplied.");
    }

    ImGui::Separator();

    ImGui::TextUnformatted(
        "Universal runtime bootstrap is active.");

    ImGui::TextDisabled(
        "The selected game's native runtime adapter will attach here in the next Jackie integration milestone.");

    ImGui::End();
}

bool WriteVerificationReport(
    const PlayerRuntimeState& state,
    const std::filesystem::path& reportPath) {

    std::ofstream report(
        reportPath,
        std::ios::binary);

    if (!report) {
        return false;
    }

    report
        << "3E Player Runtime Verification\n"
        << "==============================\n\n"
        << "Valid="
        << (state.valid ? "true" : "false")
        << "\n"
        << "RuntimeRoot="
        << state.runtimeRoot.string()
        << "\n"
        << "GameId="
        << state.gameId
        << "\n"
        << "GameDisplayName="
        << state.gameDisplayName
        << "\n"
        << "GameManifest="
        << state.gameManifest.string()
        << "\n"
        << "RuntimeHost="
        << state.runtimeHost
        << "\n"
        << "RuntimeMode="
        << state.runtimeMode
        << "\n"
        << "ProjectId="
        << state.projectId
        << "\n"
        << "ProjectManifest="
        << state.projectManifest.string()
        << "\n"
        << "GameRoot="
        << state.sourceGameRoot.string()
        << "\n"
        << "ExportedAssets="
        << state.exportedAssets.string()
        << "\n"
        << "Overlay="
        << state.overlayPath.string()
        << "\n"
        << "Error="
        << state.error
        << "\n";

    return true;
}

} // namespace threee::player