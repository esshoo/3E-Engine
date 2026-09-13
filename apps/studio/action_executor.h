#pragma once

#include "apps/studio/command_registry.h"
#include "apps/studio/project_registry.h"

#include <filesystem>
#include <string>

namespace threee::studio {

struct ActionContext {
    std::filesystem::path runtimeRoot;

    std::string activeGameId;
    std::string activeProjectId;

    const ProjectDescriptor* project = nullptr;
};

struct ActionResult {
    bool success = false;
    std::string message;
};

class ActionExecutor {
public:
    explicit ActionExecutor(std::filesystem::path runtimeRoot);

    ActionResult Execute(
        const CommandDescriptor& command,
        const ActionContext& context) const;

    std::string ExpandVariables(
        const std::string& value,
        const ActionContext& context) const;

private:
    std::filesystem::path m_runtimeRoot;

    ActionResult ExecuteAction(
        const data::JsonValue& action,
        const ActionContext& context) const;

    std::filesystem::path ResolvePath(
        const std::string& value,
        const ActionContext& context) const;

    static std::string Timestamp();
};

} // namespace threee::studio