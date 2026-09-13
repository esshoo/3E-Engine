#pragma once

#include "apps/studio/game_registry.h"
#include "apps/studio/project_registry.h"
#include "engine/data/json_value.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace threee::studio {

class StudioApp {
public:
    explicit StudioApp(std::filesystem::path runtimeRoot);

    void Update();
    void Draw();

    const std::filesystem::path& GetRuntimeRoot() const {
        return m_runtimeRoot;
    }

private:
    using Command = std::function<void()>;

    std::filesystem::path m_runtimeRoot;
    std::filesystem::path m_uiPath;

    GameRegistry m_gameRegistry;
    ProjectRegistry m_projectRegistry;

    data::JsonDocument m_uiDocument;

    std::filesystem::file_time_type m_lastWrite {};
    bool m_hasLastWrite = false;
    bool m_hasValidUi = false;

    std::chrono::steady_clock::time_point m_nextPoll {};

    std::unordered_map<std::string, Command> m_commands;

    std::string m_lastError;
    std::string m_status = "Studio host initialized.";

    std::string m_selectedGameId;
    std::string m_selectedProjectId;

    unsigned int m_reloadGeneration = 0;
    unsigned int m_commandCount = 0;

    void RegisterBuiltInCommands();
    void ReloadUi(bool force);
    void ExecuteCommand(const std::string& commandName);

    void SelectProject(const ProjectDescriptor& project);

    void DrawGameLibrary();
    void DrawProjectLibrary();
    void DrawDataDrivenWindows();
    void DrawItem(const data::JsonValue& item);
};

std::filesystem::path FindRuntimeRoot(const char* executablePath);

} // namespace threee::studio