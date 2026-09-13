#include "apps/studio/studio_app.h"

#include <imgui.h>

#include <system_error>
#include <utility>
#include <vector>

namespace threee::studio {

namespace {

bool LooksLikeProjectRoot(const std::filesystem::path& path) {
    std::error_code ec;

    return std::filesystem::exists(path / "premake5.lua", ec) &&
           std::filesystem::exists(path / "vendor" / "libp3d", ec);
}

std::filesystem::path SearchUpward(std::filesystem::path path) {
    std::error_code ec;
    path = std::filesystem::weakly_canonical(path, ec);

    if (ec) {
        ec.clear();
        path = std::filesystem::absolute(path, ec);
    }

    for (int depth = 0; depth < 10 && !path.empty(); ++depth) {
        if (LooksLikeProjectRoot(path)) {
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

} // namespace

std::filesystem::path FindProjectRoot(const char* executablePath) {
    std::error_code ec;

    if (const auto fromCurrent = SearchUpward(std::filesystem::current_path(ec));
        !fromCurrent.empty()) {
        return fromCurrent;
    }

    if (executablePath && executablePath[0]) {
        std::filesystem::path executable = executablePath;

        if (executable.is_relative()) {
            executable = std::filesystem::absolute(executable, ec);
        }

        if (const auto fromExecutable = SearchUpward(executable.parent_path());
            !fromExecutable.empty()) {
            return fromExecutable;
        }
    }

    return std::filesystem::current_path(ec);
}

StudioApp::StudioApp(std::filesystem::path projectRoot)
    : m_projectRoot(std::move(projectRoot)),
      m_uiPath(m_projectRoot / "config" / "ui" / "default" / "studio.json") {

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
        m_status = "Manual UI reload requested.";
    };
}

void StudioApp::Update() {
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

void StudioApp::DrawHostPanel() {
    ImGui::SetNextWindowSize(ImVec2(470.0f, 215.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("3E Studio Host")) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Standalone 3E-Studio executable");
    ImGui::Separator();

    ImGui::Text("Reload generation: %u", m_reloadGeneration);
    ImGui::Text("Commands executed: %u", m_commandCount);

    ImGui::TextUnformatted("Project root:");
    ImGui::TextWrapped("%s", m_projectRoot.string().c_str());

    ImGui::TextUnformatted("UI source:");
    ImGui::TextWrapped("%s", m_uiPath.string().c_str());

    if (!m_lastError.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("JSON error: %s", m_lastError.c_str());
    }

    ImGui::Separator();
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
    DrawHostPanel();
    DrawDataDrivenWindows();
}

} // namespace threee::studio