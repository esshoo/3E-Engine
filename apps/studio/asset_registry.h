#pragma once

#include "apps/studio/project_registry.h"
#include "engine/data/json_value.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace threee::studio {

struct AssetCategoryRule {
    std::string id;
    std::string label;

    std::vector<std::string> extensions;
    std::vector<std::string> pathContains;
    std::vector<std::string> excludeContains;
};

struct AssetRecord {
    std::string source;
    std::filesystem::path rootPath;
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;

    std::string extension;
    std::uintmax_t size = 0;

    std::vector<std::string> categories;
};

class AssetRegistry {
public:
    explicit AssetRegistry(std::filesystem::path runtimeRoot);

    void SetProject(
        const ProjectDescriptor* project,
        std::string gameId);

    // Update reloads rule JSON only. It intentionally does not recursively
    // rescan the asset tree every frame.
    void Update(bool force = false);

    // Refresh performs the filesystem scan explicitly.
    void Refresh(bool force = false);

    const std::vector<AssetRecord>& GetAssets() const {
        return m_assets;
    }

    const std::vector<AssetCategoryRule>& GetRules() const {
        return m_rules;
    }

    const std::string& GetLastError() const {
        return m_lastError;
    }

    const std::filesystem::path& GetRuleFile() const {
        return m_ruleFile;
    }

    unsigned int GetScanGeneration() const {
        return m_scanGeneration;
    }

    unsigned int GetRuleGeneration() const {
        return m_ruleGeneration;
    }

private:
    std::filesystem::path m_runtimeRoot;

    std::string m_projectId;
    std::string m_gameId;

    std::filesystem::path m_exportedAssets;
    std::filesystem::path m_overlay;

    std::filesystem::path m_ruleFile;
    std::filesystem::file_time_type m_ruleStamp {};
    bool m_hasRuleStamp = false;

    std::vector<AssetCategoryRule> m_rules;
    std::vector<AssetRecord> m_assets;

    std::chrono::steady_clock::time_point m_nextRuleCheck {};

    std::string m_lastError;

    unsigned int m_scanGeneration = 0;
    unsigned int m_ruleGeneration = 0;

    void ReloadRules(bool force);
    void ScanRoot(
        const std::filesystem::path& root,
        const char* sourceName);

    void Reclassify();
    void Classify(AssetRecord& asset) const;
};

} // namespace threee::studio