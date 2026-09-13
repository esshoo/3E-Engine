#pragma once

#include "engine/data/json_value.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace threee::studio {

struct CommandDescriptor {
    std::string id;
    std::string actionType;
    std::string handler;

    std::filesystem::path sourceFile;

    data::JsonValue action;

    bool IsValid() const {
        return !id.empty() &&
               !actionType.empty();
    }
};

class CommandRegistry {
public:
    explicit CommandRegistry(std::filesystem::path runtimeRoot);

    void SetActiveGame(std::string gameId);
    void Update(bool force = false);

    const CommandDescriptor* Find(
        const std::string& commandId) const;

    const std::vector<CommandDescriptor>& GetCommands() const {
        return m_commands;
    }

    const std::string& GetLastError() const {
        return m_lastError;
    }

    unsigned int GetGeneration() const {
        return m_generation;
    }

private:
    std::filesystem::path m_runtimeRoot;
    std::string m_activeGameId;

    std::vector<CommandDescriptor> m_commands;
    std::unordered_map<std::string, std::size_t> m_index;

    std::chrono::steady_clock::time_point m_nextScan {};

    std::string m_lastError;
    std::string m_signature;

    unsigned int m_generation = 0;

    void Scan();
};

} // namespace threee::studio