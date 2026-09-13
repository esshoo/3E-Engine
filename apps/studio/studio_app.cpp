#include "apps/studio/studio_app.h"

#include <imgui.h>

#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace threee::studio {

namespace {

bool LooksLikeRuntimeRoot(const std::filesystem::path& path) {
    std::error_code ec;

    const bool hasStudioConfig =
        std::filesystem::exists(path / "config" / "studio.json", ec);

    ec.clear();

    const bool hasDefaultUi =
        std::filesystem::exists(
            path / "config" / "ui" / "default" / "studio.json",
            ec);

    return hasStudioConfig || hasDefaultUi;
}

bool LooksLikeSourceRoot(const std::filesystem::path& path) {
    std::error_code ec;

    return
        std::filesystem::exists(path / "premake5.lua", ec) &&
        std::filesystem::exists(path / "vendor" / "libp3d", ec);
}

std::filesystem::path SearchUpwardForSourceRoot(std::filesystem::path path) {
    std::error_code ec;
    path = std::filesystem::weakly_canonical(path, ec);

    if (ec) {
        ec.clear();
        path = std::filesystem::absolute(path, ec);
    }

    for (int depth = 0; depth < 10 && !path.empty(); ++depth) {
        if (LooksLikeRuntimeRoot(path) || LooksLikeSourceRoot(path)) {
            return path;
        }

        const std::filesystem::path parent = path.parent_path();

        if (parent == path) {
            break;
        }

        path = parent;
    }

    return {};
}

std::filesystem::path GetExecutableDirectory(const char* executablePath) {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(32768, L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

    if (length > 0 && length < static_cast<DWORD>(buffer.size())) {
        return
            std::filesystem::path(std::wstring(buffer.data(), length))
                .parent_path();
    }
#endif

    std::error_code ec;

    if (executablePath && executablePath[0]) {
        std::filesystem::path executable(executablePath);

        if (executable.is_relative()) {
            executable = std::filesystem::absolute(executable, ec);
        }

        if (!ec) {
            return executable.parent_path();
        }
    }

    return {};
}

} // namespace

std::filesystem::path FindRuntimeRoot(const char* executablePath) {
    std::error_code ec;

    const std::filesystem::path executableDirectory =
        GetExecutableDirectory(executablePath);

    if (!executableDirectory.empty() && LooksLikeRuntimeRoot(executableDirectory)) {
        return executableDirectory;
    }

    const std::filesystem::path currentDirectory =
        std::filesystem::current_path(ec);

    if (!ec && LooksLikeRuntimeRoot(currentDirectory)) {
        return currentDirectory;
    }

    if (!currentDirectory.empty()) {
        if (const auto sourceRoot = SearchUpwardForSourceRoot(currentDirectory);
            !sourceRoot.empty()) {
            return sourceRoot;
        }
    }

    if (!executableDirectory.empty()) {
        if (const auto sourceRoot = SearchUpwardForSourceRoot(executableDirectory);
            !sourceRoot.empty()) {
            return sourceRoot;
        }

        return executableDirectory;
    }

    return currentDirectory;
}

StudioApp::StudioApp(std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)),
      m_uiPath(m_runtimeRoot / "config" / "ui" / "default" / "studio.json"),
      m_gameRegistry(m_runtimeRoot) {

    RegisterBuiltInCommands();
    ReloadUi(true);
}

void StudioApp::RegisterBuiltInCommands() {
    m_commands["test.live_reload"] = [this]() {
        ++m_commandCount;
        m_status =
            "test.live_reload executed. Command count: " +
            std::to_string(m_commandCount);
    };

    m_commands["studio.reload"] = [this]() {
        ReloadUi(true);
        m_gameRegistry.Update(true);
        m_status = "Studio data reloaded.";
    };
}

void StudioApp::Update() {
    m_gameRegistry.Update();

    const auto now = std::chrono::steady_clock::now();

    if (now < m_nextPoll) {
        return;
    }

    m_nextPoll = now + std::chrono::milliseconds(250);
    ReloadUi(false);
}

void StudioApp::ReloadUi(bool force) {
    std::error_code ec;

    if (!std::filesystem::exists(m_uiPath, ec)) {
        m_lastError = "UI file not found: " + m_uiPath.string();
        m_hasLastWrite = false;
        return;
    }

    const auto writeTime = std::filesystem::last_write_time(m_uiPath, ec);

    if (ec) {
        m_lastError = "Cannot read UI timestamp: " + ec.message();
        return;
    }

    if (!force && m_hasLastWrite && writeTime == m_lastWrite) {
        return;
    }

    m_lastWrite = writeTime;
    m_hasLastWrite = true;

    data::JsonDocument candidate = data::JsonDocument::LoadFile(m_uiPath);

    if (!candidate.Ok()) {
        m_lastError = candidate.error;
        return;
    }

    m_uiDocument = std::move(candidate);
    m_hasValidUi = true;
    m_lastError.clear();

    ++m_reloadGeneration;
    m_status =
        "UI loaded successfully. Reload generation: " +
        std::to_string(m_reloadGeneration);
}

void StudioApp::ExecuteCommand(const std::string& commandName) {
    const auto it = m_commands.find(commandName);

    if (it == m_commands.end()) {
        m_status = "Unknown command: " + commandName;
        return;
    }

    it->second();
}

void StudioApp::DrawGameLibrary() {
    ImGui::SetNextWindowSize(ImVec2(720.0f, 520.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3E Studio - Games")) {
        ImGui::End();
        return;
    }

    const auto& games = m_gameRegistry.GetGames();

    ImGui::TextUnformatted("Game Library");
    ImGui::Separator();

    ImGui::Text("Discovered games: %d", static_cast<int>(games.size()));
    ImGui::TextWrapped("Runtime root: %s", m_runtimeRoot.string().c_str());
    ImGui::TextWrapped(
        "Games directory: %s",
        m_gameRegistry.GetGamesDirectory().string().c_str());

    ImGui::Separator();

    if (games.empty()) {
        ImGui::TextDisabled("No enabled games were discovered.");
    }

    for (const GameDescriptor& game : games) {
        ImGui::PushID(game.id.c_str());

        const bool selected = m_selectedGameId == game.id;

        if (ImGui::Selectable(game.displayName.c_str(), selected)) {
            m_selectedGameId = game.id;
            m_status = "Selected game: " + game.displayName;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", game.id.c_str());

        if (selected) {
            const char* integration =
                game.integrationState.empty()
                    ? "unspecified"
                    : game.integrationState.c_str();

            ImGui::Text("Integration: %s", integration);
            ImGui::TextWrapped("Game root: %s", game.rootPath.string().c_str());
            ImGui::TextWrapped("Manifest: %s", game.manifestPath.string().c_str());
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    const auto& invalidGames = m_gameRegistry.GetInvalidGames();

    if (!invalidGames.empty() && ImGui::CollapsingHeader("Invalid Game Manifests")) {
        for (const GameDescriptor& game : invalidGames) {
            const std::string display = game.displayName.empty()
                ? game.manifestPath.string()
                : game.displayName;

            ImGui::BulletText("%s: %s", display.c_str(), game.error.c_str());
        }
    }

    if (!m_gameRegistry.GetLastError().empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Game Registry: %s", m_gameRegistry.GetLastError().c_str());
    }

    if (!m_lastError.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("UI JSON: %s", m_lastError.c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Refresh Games")) {
        m_gameRegistry.Update(true);
        m_status = "Game Registry refreshed.";
    }

    ImGui::SameLine();
    ImGui::TextWrapped("%s", m_status.c_str());

    ImGui::End();
}

void StudioApp::DrawItem(const data::JsonValue& item) {
    if (!item.IsObject()) {
        return;
    }

    const std::string type = item.GetString("type");

    if (type == "text") {
        const std::string label = item.GetString("label");
        ImGui::TextUnformatted(label.c_str());
        return;
    }

    if (type == "bullet") {
        const std::string label = item.GetString("label");
        ImGui::BulletText("%s", label.c_str());
        return;
    }

    if (type == "separator") {
        ImGui::Separator();
        return;
    }

    if (type == "spacing") {
        ImGui::Spacing();
        return;
    }

    if (type == "same_line") {
        ImGui::SameLine();
        return;
    }

    if (type == "button") {
        const std::string label = item.GetString("label", "Button");
        const std::string command = item.GetString("command");

        if (ImGui::Button(label.c_str())) {
            ExecuteCommand(command);
        }

        return;
    }

    ImGui::TextDisabled(
        "Unsupported UI item type: %s",
        type.empty() ? "<missing>" : type.c_str());
}

void StudioApp::DrawDataDrivenWindows() {
    if (!m_hasValidUi || !m_uiDocument.root.IsObject()) {
        return;
    }

    const data::JsonValue* windows = m_uiDocument.root.Find("windows");

    if (!windows || !windows->IsArray()) {
        return;
    }

    for (const data::JsonValue& window : windows->arrayValue) {
        if (!window.IsObject()) {
            continue;
        }

        const std::string title = window.GetString("title", "Untitled");

        ImGui::SetNextWindowSize(ImVec2(500.0f, 260.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(title.c_str())) {
            const data::JsonValue* items = window.Find("items");

            if (items && items->IsArray()) {
                for (const data::JsonValue& item : items->arrayValue) {
                    DrawItem(item);
                }
            }
        }

        ImGui::End();
    }
}

void StudioApp::Draw() {
    DrawGameLibrary();
    DrawDataDrivenWindows();
}

} // namespace threee::studio