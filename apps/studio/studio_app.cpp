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
      m_uiPath(
          m_runtimeRoot /
          "config" /
          "ui" /
          "default" /
          "studio.json"),
      m_gameRegistry(m_runtimeRoot),
      m_projectRegistry(m_runtimeRoot),
      m_commandRegistry(m_runtimeRoot),
      m_uiRegistry(m_runtimeRoot),
      m_actionExecutor(m_runtimeRoot) {

    RegisterBuiltInCommands();
    ReloadUi(true);

    const auto& projects =
        m_projectRegistry.GetProjects();

    if (projects.size() == 1) {
        SelectProject(projects.front());
    }
}

void StudioApp::RegisterBuiltInCommands() {
    m_builtinHandlers["studio.command_palette"] = [this]() {
        m_showCommandPalette = true;
        m_status = "Command Palette opened.";
    };

    m_builtinHandlers["studio.reload_definitions"] = [this]() {
        ReloadUi(true);
        m_gameRegistry.Update(true);
        m_projectRegistry.Update(true);
        m_commandRegistry.Update(true);
        m_uiRegistry.Update(true);

        m_status = "All Studio definitions reloaded.";
    };

    m_builtinHandlers["project.open"] = [this]() {
        m_status = "Select a project from Project Library.";
    };

    m_builtinHandlers["game.play"] = [this]() {
        m_status = "Play command is registered; runtime launch comes in a later foundation stage.";
    };

    m_builtinHandlers["game.stop"] = [this]() {
        m_status = "Stop command is registered.";
    };

    m_builtinHandlers["edit.undo"] = [this]() {
        m_status = "Undo is registered; editor transaction stack is not active yet.";
    };

    m_builtinHandlers["edit.redo"] = [this]() {
        m_status = "Redo is registered; editor transaction stack is not active yet.";
    };

    m_builtinHandlers["studio.about"] = [this]() {
        m_status = "3E Studio - data-driven reverse-engineering workspace.";
    };

    m_builtinHandlers["test.live_reload"] = [this]() {
        ++m_commandCount;

        m_status =
            "test.live_reload executed. Command count: " +
            std::to_string(m_commandCount);
    };

    m_builtinHandlers["studio.reload"] = [this]() {
        ReloadUi(true);
        m_gameRegistry.Update(true);
        m_projectRegistry.Update(true);
        m_commandRegistry.Update(true);
        m_uiRegistry.Update(true);

        m_status = "Studio data reloaded.";
    };

    m_builtinHandlers["project.refresh"] = [this]() {
        m_projectRegistry.Update(true);
        m_status = "Project Registry refreshed.";
    };
}

void StudioApp::SetActiveGame(
    const std::string& gameId) {

    if (m_selectedGameId == gameId) {
        return;
    }

    m_selectedGameId = gameId;

    m_commandRegistry.SetActiveGame(gameId);
    m_uiRegistry.SetActiveGame(gameId);
}

void StudioApp::SelectProject(
    const ProjectDescriptor& project) {

    m_selectedProjectId = project.id;
    SetActiveGame(project.gameId);

    m_status =
        "Active project: " +
        project.displayName;
}

void StudioApp::Update() {
    m_gameRegistry.Update();
    m_projectRegistry.Update();
    m_commandRegistry.Update();
    m_uiRegistry.Update();

    const auto now =
        std::chrono::steady_clock::now();

    if (now < m_nextPoll) {
        return;
    }

    m_nextPoll =
        now + std::chrono::milliseconds(250);

    ReloadUi(false);
}

void StudioApp::ReloadUi(bool force) {
    std::error_code ec;

    if (!std::filesystem::exists(m_uiPath, ec)) {
        m_lastError =
            "UI file not found: " +
            m_uiPath.string();

        m_hasLastWrite = false;
        return;
    }

    const auto writeTime =
        std::filesystem::last_write_time(
            m_uiPath,
            ec);

    if (ec) {
        m_lastError =
            "Cannot read UI timestamp: " +
            ec.message();

        return;
    }

    if (!force &&
        m_hasLastWrite &&
        writeTime == m_lastWrite) {
        return;
    }

    m_lastWrite = writeTime;
    m_hasLastWrite = true;

    data::JsonDocument candidate =
        data::JsonDocument::LoadFile(m_uiPath);

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

const ProjectDescriptor* StudioApp::FindActiveProject() const {
    if (m_selectedProjectId.empty()) {
        return nullptr;
    }

    for (const ProjectDescriptor& project :
         m_projectRegistry.GetProjects()) {

        if (project.id == m_selectedProjectId) {
            return &project;
        }
    }

    return nullptr;
}

ActionContext StudioApp::BuildActionContext() const {
    ActionContext context;

    context.runtimeRoot = m_runtimeRoot;
    context.activeGameId = m_selectedGameId;
    context.activeProjectId = m_selectedProjectId;
    context.project = FindActiveProject();

    return context;
}

void StudioApp::ExecuteResolvedCommand(
    const CommandDescriptor& command) {

    if (command.actionType == "builtin") {
        const std::string handler =
            command.handler.empty()
                ? command.id
                : command.handler;

        const auto builtin =
            m_builtinHandlers.find(handler);

        if (builtin == m_builtinHandlers.end()) {
            m_status =
                "Builtin handler not implemented yet: " +
                handler;
            return;
        }

        builtin->second();
        return;
    }

    if (command.actionType == "game") {
        m_status =
            "Game command resolved: " +
            command.id +
            " -> " +
            command.handler;

        return;
    }

    const ActionResult result =
        m_actionExecutor.Execute(
            command,
            BuildActionContext());

    m_status =
        result.success
            ? result.message
            : "ERROR: " + result.message;
}

void StudioApp::ExecuteCommand(
    const std::string& commandId) {

    if (commandId.empty()) {
        return;
    }

    const CommandDescriptor* command =
        m_commandRegistry.Find(commandId);

    if (command) {
        ExecuteResolvedCommand(*command);
        return;
    }

    // Allow direct host handlers for lightweight UI commands that
    // have not been declared in a command JSON file yet.
    const auto direct =
        m_builtinHandlers.find(commandId);

    if (direct != m_builtinHandlers.end()) {
        direct->second();
        return;
    }

    m_status =
        "Unknown command: " +
        commandId;
}

void StudioApp::DrawDynamicMenuBar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    for (const UiMenu& menu :
         m_uiRegistry.GetMenus()) {

        const std::string label =
            menu.label.empty()
                ? menu.id
                : menu.label;

        if (!ImGui::BeginMenu(label.c_str())) {
            continue;
        }

        for (const UiCommandItem& item :
             menu.items) {

            if (item.type == "separator") {
                ImGui::Separator();
                continue;
            }

            if (item.type != "command") {
                continue;
            }

            const std::string itemLabel =
                item.label.empty()
                    ? item.command
                    : item.label;

            if (ImGui::MenuItem(itemLabel.c_str())) {
                ExecuteCommand(item.command);
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void StudioApp::DrawDynamicToolbar() {
    ImGui::SetNextWindowSize(
        ImVec2(700.0f, 70.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3E Studio - Toolbar")) {
        ImGui::End();
        return;
    }

    bool first = true;

    for (const UiCommandItem& item :
         m_uiRegistry.GetToolbarItems()) {

        if (item.type == "separator") {
            if (!first) {
                ImGui::SameLine();
            }

            ImGui::TextDisabled("|");
            first = false;
            continue;
        }

        if (item.type != "command") {
            continue;
        }

        if (!first) {
            ImGui::SameLine();
        }

        const std::string label =
            item.label.empty()
                ? item.command
                : item.label;

        if (ImGui::Button(label.c_str())) {
            ExecuteCommand(item.command);
        }

        first = false;
    }

    ImGui::End();
}

void StudioApp::DrawCommandPalette() {
    if (!m_showCommandPalette) {
        return;
    }

    ImGui::SetNextWindowSize(
        ImVec2(620.0f, 500.0f),
        ImGuiCond_Appearing);

    if (!ImGui::Begin(
            "3E Studio - Command Palette",
            &m_showCommandPalette)) {

        ImGui::End();
        return;
    }

    ImGui::Text(
        "Commands: %d",
        static_cast<int>(
            m_commandRegistry
                .GetCommands()
                .size()));

    ImGui::Separator();

    for (const CommandDescriptor& command :
         m_commandRegistry.GetCommands()) {

        if (ImGui::Selectable(
                command.id.c_str())) {

            ExecuteResolvedCommand(command);
            m_showCommandPalette = false;
        }

        ImGui::SameLine();

        ImGui::TextDisabled(
            "[%s -> %s]",
            command.actionType.c_str(),
            command.handler.c_str());
    }

    if (!m_commandRegistry
             .GetLastError()
             .empty()) {

        ImGui::Separator();

        ImGui::TextWrapped(
            "Command Registry: %s",
            m_commandRegistry
                .GetLastError()
                .c_str());
    }

    ImGui::End();
}

void StudioApp::DrawGameLibrary() {
    ImGui::SetNextWindowSize(
        ImVec2(720.0f, 420.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3E Studio - Games")) {
        ImGui::End();
        return;
    }

    const auto& games =
        m_gameRegistry.GetGames();

    ImGui::TextUnformatted("Game Library");
    ImGui::Separator();

    ImGui::Text(
        "Discovered games: %d",
        static_cast<int>(games.size()));

    ImGui::TextWrapped(
        "Runtime root: %s",
        m_runtimeRoot.string().c_str());

    ImGui::TextWrapped(
        "Games directory: %s",
        m_gameRegistry
            .GetGamesDirectory()
            .string()
            .c_str());

    ImGui::Separator();

    if (games.empty()) {
        ImGui::TextDisabled(
            "No enabled games were discovered.");
    }

    for (const GameDescriptor& game : games) {
        ImGui::PushID(game.id.c_str());

        const bool selected =
            m_selectedGameId == game.id;

        if (ImGui::Selectable(
                game.displayName.c_str(),
                selected)) {

            SetActiveGame(game.id);

            m_status =
                "Selected game: " +
                game.displayName;
        }

        ImGui::SameLine();

        ImGui::TextDisabled(
            "[%s]",
            game.id.c_str());

        if (selected) {
            const char* integration =
                game.integrationState.empty()
                    ? "unspecified"
                    : game.integrationState.c_str();

            ImGui::Text(
                "Integration: %s",
                integration);

            ImGui::TextWrapped(
                "Game definition root: %s",
                game.rootPath.string().c_str());

            ImGui::TextWrapped(
                "Manifest: %s",
                game.manifestPath.string().c_str());
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    const auto& invalidGames =
        m_gameRegistry.GetInvalidGames();

    if (!invalidGames.empty() &&
        ImGui::CollapsingHeader(
            "Invalid Game Manifests")) {

        for (const GameDescriptor& game :
             invalidGames) {

            const std::string display =
                game.displayName.empty()
                    ? game.manifestPath.string()
                    : game.displayName;

            ImGui::BulletText(
                "%s: %s",
                display.c_str(),
                game.error.c_str());
        }
    }

    if (!m_gameRegistry
             .GetLastError()
             .empty()) {

        ImGui::Separator();

        ImGui::TextWrapped(
            "Game Registry: %s",
            m_gameRegistry
                .GetLastError()
                .c_str());
    }

    if (!m_uiRegistry.GetLastError().empty()) {
        ImGui::Separator();

        ImGui::TextWrapped(
            "UI Registry: %s",
            m_uiRegistry.GetLastError().c_str());
    }

    ImGui::End();
}

void StudioApp::DrawProjectLibrary() {
    ImGui::SetNextWindowSize(
        ImVec2(760.0f, 520.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3E Studio - Projects")) {
        ImGui::End();
        return;
    }

    const auto& projects =
        m_projectRegistry.GetProjects();

    ImGui::TextUnformatted("Project Library");
    ImGui::Separator();

    ImGui::Text(
        "Discovered projects: %d",
        static_cast<int>(projects.size()));

    ImGui::TextWrapped(
        "Projects directory: %s",
        m_projectRegistry
            .GetProjectsDirectory()
            .string()
            .c_str());

    ImGui::Separator();

    if (projects.empty()) {
        ImGui::TextDisabled(
            "No valid 3E projects were discovered.");
    }

    for (const ProjectDescriptor& project :
         projects) {

        ImGui::PushID(project.id.c_str());

        const bool selected =
            m_selectedProjectId == project.id;

        if (ImGui::Selectable(
                project.displayName.c_str(),
                selected)) {

            SelectProject(project);
        }

        ImGui::SameLine();

        ImGui::TextDisabled(
            "[%s -> %s]",
            project.id.c_str(),
            project.gameId.c_str());

        if (selected) {
            ImGui::TextUnformatted(
                "ACTIVE PROJECT");

            ImGui::TextWrapped(
                "Project root: %s",
                project.projectRoot
                    .string()
                    .c_str());

            ImGui::TextWrapped(
                "Manifest: %s",
                project.manifestPath
                    .string()
                    .c_str());

            ImGui::Separator();

            ImGui::TextWrapped(
                "Game root (READ ONLY): %s",
                project.gameRoot
                    .string()
                    .c_str());

            ImGui::TextWrapped(
                "ExportedAssets (READ ONLY): %s",
                project.exportedAssets
                    .string()
                    .c_str());

            ImGui::Separator();

            ImGui::TextWrapped(
                "Overlay: %s",
                project.overlayPath
                    .string()
                    .c_str());

            ImGui::TextWrapped(
                "Cache: %s",
                project.cachePath
                    .string()
                    .c_str());

            ImGui::TextWrapped(
                "Temp: %s",
                project.tempPath
                    .string()
                    .c_str());
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    const auto& invalidProjects =
        m_projectRegistry.GetInvalidProjects();

    if (!invalidProjects.empty() &&
        ImGui::CollapsingHeader(
            "Invalid Projects")) {

        for (const ProjectDescriptor& project :
             invalidProjects) {

            const std::string display =
                project.displayName.empty()
                    ? project.manifestPath.string()
                    : project.displayName;

            ImGui::BulletText(
                "%s: %s",
                display.c_str(),
                project.error.c_str());
        }
    }

    if (!m_projectRegistry
             .GetLastError()
             .empty()) {

        ImGui::Separator();

        ImGui::TextWrapped(
            "Project Registry: %s",
            m_projectRegistry
                .GetLastError()
                .c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Refresh Projects")) {
        m_projectRegistry.Update(true);
        m_status = "Project Registry refreshed.";
    }

    ImGui::SameLine();
    ImGui::TextWrapped("%s", m_status.c_str());

    ImGui::End();
}

void StudioApp::DrawItem(
    const data::JsonValue& item) {

    if (!item.IsObject()) {
        return;
    }

    const std::string type =
        item.GetString("type");

    if (type == "text") {
        const std::string label =
            item.GetString("label");

        ImGui::TextUnformatted(
            label.c_str());

        return;
    }

    if (type == "bullet") {
        const std::string label =
            item.GetString("label");

        ImGui::BulletText(
            "%s",
            label.c_str());

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
        const std::string label =
            item.GetString(
                "label",
                "Button");

        const std::string command =
            item.GetString("command");

        if (ImGui::Button(label.c_str())) {
            ExecuteCommand(command);
        }

        return;
    }

    ImGui::TextDisabled(
        "Unsupported UI item type: %s",
        type.empty()
            ? "<missing>"
            : type.c_str());
}

void StudioApp::DrawDataDrivenWindows() {
    if (!m_hasValidUi ||
        !m_uiDocument.root.IsObject()) {
        return;
    }

    const data::JsonValue* windows =
        m_uiDocument.root.Find("windows");

    if (!windows || !windows->IsArray()) {
        return;
    }

    for (const data::JsonValue& window :
         windows->arrayValue) {

        if (!window.IsObject()) {
            continue;
        }

        const std::string title =
            window.GetString(
                "title",
                "Untitled");

        ImGui::SetNextWindowSize(
            ImVec2(500.0f, 260.0f),
            ImGuiCond_FirstUseEver);

        if (ImGui::Begin(title.c_str())) {
            const data::JsonValue* items =
                window.Find("items");

            if (items && items->IsArray()) {
                for (const data::JsonValue& item :
                     items->arrayValue) {

                    DrawItem(item);
                }
            }
        }

        ImGui::End();
    }
}

void StudioApp::Draw() {
    DrawDynamicMenuBar();
    DrawDynamicToolbar();
    DrawGameLibrary();
    DrawProjectLibrary();
    DrawCommandPalette();
    DrawDataDrivenWindows();
}

} // namespace threee::studio