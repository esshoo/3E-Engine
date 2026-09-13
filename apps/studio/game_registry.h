#pragma once

#include "engine/data/json_value.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace threee::studio {

struct GameDescriptor {
    std::string id;
    std::string displayName;
    std::string integrationState;

    std::filesystem::path rootPath;
    std::filesystem::path manifestPath;

    std::uint64_t manifestStamp = 0;
    bool enabled = true;

    std::string error;

    bool IsValid() const {
        return error.empty() && !id.empty() && !displayName.empty();
    }
};

class GameRegistry {
public:
    explicit GameRegistry(std::filesystem::path runtimeRoot);

    void Update(bool force = false);

    const std::vector<GameDescriptor>& GetGames() const {
        return m_games;
    }

    const std::vector<GameDescriptor>& GetInvalidGames() const {
        return m_invalidGames;
    }

    const std::filesystem::path& GetGamesDirectory() const {
        return m_gamesDirectory;
    }

    const std::string& GetLastError() const {
        return m_lastError;
    }

    unsigned int GetGeneration() const {
        return m_generation;
    }

private:
    std::filesystem::path m_runtimeRoot;
    std::filesystem::path m_studioConfigPath;
    std::filesystem::path m_gamesDirectory;

    std::vector<GameDescriptor> m_games;
    std::vector<GameDescriptor> m_invalidGames;

    std::chrono::steady_clock::time_point m_nextScan {};

    std::string m_lastError;
    std::string m_signature;

    unsigned int m_generation = 0;

    void Scan();
};

} // namespace threee::studio