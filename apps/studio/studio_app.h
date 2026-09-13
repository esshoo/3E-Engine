#pragma once

#include "apps/studio/action_executor.h"
#include "apps/studio/asset_registry.h"
#include "apps/studio/command_registry.h"
#include "apps/studio/game_registry.h"
#include "apps/studio/project_registry.h"
#include "apps/studio/ui_registry.h"
#include "engine/data/json_value.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace threee::studio {

class StudioApp {
public:
    explicit StudioApp(std::filesystem::path runtimeRoot);
    ~StudioApp();

    void Update();
    void Draw();

    const std::filesystem::path& GetRuntimeRoot() const {
        return m_runtimeRoot;
    }

    bool ShouldExit() const {
        return m_exitRequested;
    }

private:
    using BuiltinCommand = std::function<void()>;

    std::filesystem::path m_runtimeRoot;
    std::filesystem::path m_uiPath;

    GameRegistry m_gameRegistry;
    ProjectRegistry m_projectRegistry;
    CommandRegistry m_commandRegistry;
    UiRegistry m_uiRegistry;
    ActionExecutor m_actionExecutor;
    AssetRegistry m_assetRegistry;

    data::JsonDocument m_uiDocument;

    std::filesystem::file_time_type m_lastWrite {};
    bool m_hasLastWrite = false;
    bool m_hasValidUi = false;

    std::chrono::steady_clock::time_point m_nextPoll {};

    std::unordered_map<std::string, BuiltinCommand> m_builtinHandlers;
    std::unordered_map<std::string, bool> m_panelVisibility;

    std::string m_lastError;
    std::string m_status = "Studio host initialized.";

    std::string m_selectedGameId;
    std::string m_selectedProjectId;

    std::string m_assetCategory = "all";
    std::string m_selectedAssetPath;
    std::array<char, 256> m_assetSearch {};

    bool m_showCommandPalette = false;
    bool m_exitRequested = false;

    void* m_playerProcessHandle = nullptr;
    unsigned long m_playerProcessId = 0;

    unsigned int m_reloadGeneration = 0;
    unsigned int m_commandCount = 0;
    unsigned int m_lastUiGeneration = 0;

    void RegisterBuiltInCommands();
    void ReloadUi(bool force);

    void SetActiveGame(const std::string& gameId);
    void SelectProject(const ProjectDescriptor& project);

    const ProjectDescriptor* FindActiveProject() const;
    ActionContext BuildActionContext() const;

    std::filesystem::path ResolvePlayerExecutable() const;
    void LaunchPlayer();
    void StopPlayer();
    void PollPlayerProcess();

    void ExecuteCommand(const std::string& commandId);
    void ExecuteResolvedCommand(const CommandDescriptor& command);

    void SyncPanelVisibility();
    bool TogglePanelCommand(const std::string& commandId);

    void ProcessShortcuts();
    bool IsShortcutPressed(const std::string& expression) const;

    void ApplyPanelLayoutHint(const UiPanel& panel) const;
    void DrawDynamicPanels();
    void DrawPlaceholderPanel(const UiPanel& panel);

    void DrawDynamicMenuBar();
    void DrawDynamicToolbar();
    void DrawCommandPalette();

    void DrawGameLibrary();
    void DrawProjectLibrary();
    void DrawAssetBrowser();
    void DrawDataDrivenWindows();
    void DrawItem(const data::JsonValue& item);
};


} // namespace threee::studio