#include "apps/studio/asset_registry.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace threee::studio {

namespace {

std::string Lower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return value;
}

bool ContainsInsensitive(
    const std::string& haystack,
    const std::string& needle) {

    if (needle.empty()) {
        return true;
    }

    return
        Lower(haystack).find(
            Lower(needle))
        != std::string::npos;
}

bool MatchesAny(
    const std::string& value,
    const std::vector<std::string>& candidates) {

    if (candidates.empty()) {
        return true;
    }

    for (const std::string& candidate : candidates) {
        if (ContainsInsensitive(value, candidate)) {
            return true;
        }
    }

    return false;
}

bool ExtensionMatches(
    const std::string& extension,
    const std::vector<std::string>& extensions) {

    if (extensions.empty()) {
        return true;
    }

    const std::string normalized =
        Lower(extension);

    for (std::string expected : extensions) {
        expected = Lower(expected);

        if (!expected.empty() &&
            expected[0] != '.') {
            expected.insert(
                expected.begin(),
                '.');
        }

        if (normalized == expected) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> ReadStringArray(
    const data::JsonValue& object,
    const char* key) {

    std::vector<std::string> result;

    const data::JsonValue* array =
        object.Find(key);

    if (!array || !array->IsArray()) {
        return result;
    }

    for (const data::JsonValue& value :
         array->arrayValue) {

        if (value.IsString()) {
            result.push_back(
                value.stringValue);
        }
    }

    return result;
}

} // namespace

AssetRegistry::AssetRegistry(
    std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)) {
}

void AssetRegistry::SetProject(
    const ProjectDescriptor* project,
    std::string gameId) {

    const std::string newProjectId =
        project ? project->id : std::string {};

    const std::filesystem::path newExportedAssets =
        project
            ? project->exportedAssets
            : std::filesystem::path {};

    const std::filesystem::path newOverlay =
        project
            ? project->overlayPath
            : std::filesystem::path {};

    if (m_projectId == newProjectId &&
        m_gameId == gameId &&
        m_exportedAssets == newExportedAssets &&
        m_overlay == newOverlay) {
        return;
    }

    m_projectId = newProjectId;
    m_gameId = std::move(gameId);
    m_exportedAssets = newExportedAssets;
    m_overlay = newOverlay;

    m_ruleFile =
        m_gameId.empty()
            ? std::filesystem::path {}
            : m_runtimeRoot /
                "games" /
                m_gameId /
                "schemas" /
                "asset_rules.json";

    m_hasRuleStamp = false;
    m_rules.clear();
    m_assets.clear();
    m_lastError.clear();

    ReloadRules(true);

    if (project) {
        Refresh(true);
    }
}

void AssetRegistry::Update(bool force) {
    const auto now =
        std::chrono::steady_clock::now();

    if (!force && now < m_nextRuleCheck) {
        return;
    }

    m_nextRuleCheck =
        now + std::chrono::milliseconds(250);

    ReloadRules(force);
}

void AssetRegistry::ReloadRules(bool force) {
    if (m_ruleFile.empty()) {
        return;
    }

    std::error_code ec;

    if (!std::filesystem::exists(
            m_ruleFile,
            ec)) {

        if (force) {
            m_rules.clear();
            Reclassify();

            m_lastError =
                "Asset rule file not found: " +
                m_ruleFile.string();
        }

        return;
    }

    const auto stamp =
        std::filesystem::last_write_time(
            m_ruleFile,
            ec);

    if (ec) {
        m_lastError =
            "Cannot read asset rule timestamp: " +
            ec.message();
        return;
    }

    if (!force &&
        m_hasRuleStamp &&
        stamp == m_ruleStamp) {
        return;
    }

    const data::JsonDocument document =
        data::JsonDocument::LoadFile(
            m_ruleFile);

    if (!document.Ok()) {
        m_lastError =
            "Cannot parse asset rules: " +
            document.error;
        return;
    }

    std::vector<AssetCategoryRule> rules;

    if (document.root.IsObject()) {
        const data::JsonValue* categories =
            document.root.Find("categories");

        if (categories &&
            categories->IsArray()) {

            for (const data::JsonValue& value :
                 categories->arrayValue) {

                if (!value.IsObject()) {
                    continue;
                }

                AssetCategoryRule rule;

                rule.id =
                    value.GetString("id");

                rule.label =
                    value.GetString(
                        "label",
                        rule.id);

                rule.extensions =
                    ReadStringArray(
                        value,
                        "extensions");

                rule.pathContains =
                    ReadStringArray(
                        value,
                        "pathContains");

                rule.excludeContains =
                    ReadStringArray(
                        value,
                        "excludeContains");

                if (!rule.id.empty()) {
                    rules.push_back(
                        std::move(rule));
                }
            }
        }
    }

    m_ruleStamp = stamp;
    m_hasRuleStamp = true;
    m_rules = std::move(rules);
    m_lastError.clear();

    ++m_ruleGeneration;

    Reclassify();
}

void AssetRegistry::Refresh(bool force) {
    (void)force;

    m_assets.clear();
    m_lastError.clear();

    if (m_projectId.empty()) {
        return;
    }

    ScanRoot(
        m_exportedAssets,
        "ExportedAssets");

    ScanRoot(
        m_overlay,
        "Overlay");

    std::sort(
        m_assets.begin(),
        m_assets.end(),
        [](const AssetRecord& a,
           const AssetRecord& b) {

            if (a.source == b.source) {
                return
                    a.relativePath.generic_string()
                    <
                    b.relativePath.generic_string();
            }

            return a.source < b.source;
        });

    Reclassify();

    ++m_scanGeneration;
}

void AssetRegistry::ScanRoot(
    const std::filesystem::path& root,
    const char* sourceName) {

    if (root.empty()) {
        return;
    }

    std::error_code ec;

    if (!std::filesystem::exists(root, ec)) {
        if (!m_lastError.empty()) {
            m_lastError += "\n";
        }

        m_lastError +=
            std::string(sourceName) +
            " root not found: " +
            root.string();

        return;
    }

    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        ec);

    if (ec) {
        if (!m_lastError.empty()) {
            m_lastError += "\n";
        }

        m_lastError +=
            "Cannot scan " +
            root.string() +
            ": " +
            ec.message();

        return;
    }

    constexpr std::size_t kSafetyLimit = 250000;

    for (const auto& entry : iterator) {
        if (m_assets.size() >= kSafetyLimit) {
            if (!m_lastError.empty()) {
                m_lastError += "\n";
            }

            m_lastError +=
                "Asset scan stopped at safety limit: " +
                std::to_string(kSafetyLimit);

            break;
        }

        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        AssetRecord asset;

        asset.source = sourceName;
        asset.rootPath = root;
        asset.absolutePath = entry.path();

        asset.relativePath =
            std::filesystem::relative(
                entry.path(),
                root,
                ec);

        if (ec) {
            ec.clear();

            asset.relativePath =
                entry.path().filename();
        }

        asset.extension =
            Lower(
                entry.path()
                    .extension()
                    .string());

        asset.size =
            entry.file_size(ec);

        if (ec) {
            ec.clear();
            asset.size = 0;
        }

        m_assets.push_back(
            std::move(asset));
    }
}

void AssetRegistry::Reclassify() {
    for (AssetRecord& asset : m_assets) {
        Classify(asset);
    }
}

void AssetRegistry::Classify(
    AssetRecord& asset) const {

    asset.categories.clear();

    const std::string pathText =
        asset.relativePath.generic_string();

    for (const AssetCategoryRule& rule :
         m_rules) {

        if (!ExtensionMatches(
                asset.extension,
                rule.extensions)) {
            continue;
        }

        if (!MatchesAny(
                pathText,
                rule.pathContains)) {
            continue;
        }

        bool excluded = false;

        for (const std::string& token :
             rule.excludeContains) {

            if (ContainsInsensitive(
                    pathText,
                    token)) {

                excluded = true;
                break;
            }
        }

        if (excluded) {
            continue;
        }

        asset.categories.push_back(
            rule.id);
    }

    if (asset.categories.empty()) {
        asset.categories.push_back(
            "other");
    }
}

} // namespace threee::studio