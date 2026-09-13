#pragma once

#include "engine/data/json_value.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace threee::studio {

struct ProjectDescriptor {
    std::string id;
    std::string displayName;
    std::string gameId;

    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;

    std::filesystem::path gameRoot;
    std::filesystem::path exportedAssets;

    std::filesystem::path overlayPath;
    std::filesystem::path cachePath;
    std::filesystem::path tempPath;

    bool sourceReadOnly = true;
    std::uint64_t manifestStamp = 0;

    std::string error;

    bool IsValid() const {
        return error.empty() &&
               !id.empty() &&
               !displayName.empty() &&
               !gameId.empty();
    }
};

class ProjectRegistry {
public:
    explicit ProjectRegistry(std::filesystem::path runtimeRoot);

    void Update(bool force = false);

    const std::vector<ProjectDescriptor>& GetProjects() const {
        return m_projects;
    }

    const std::vector<ProjectDescriptor>& GetInvalidProjects() const {
        return m_invalidProjects;
    }

    const std::filesystem::path& GetProjectsDirectory() const {
        return m_projectsDirectory;
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
    std::filesystem::path m_projectsDirectory;

    std::vector<ProjectDescriptor> m_projects;
    std::vector<ProjectDescriptor> m_invalidProjects;

    std::chrono::steady_clock::time_point m_nextScan {};

    std::string m_lastError;
    std::string m_signature;

    unsigned int m_generation = 0;

    void Scan();
};

} // namespace threee::studio