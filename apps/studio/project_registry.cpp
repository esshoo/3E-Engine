#include "apps/studio/project_registry.h"

#include <algorithm>
#include <sstream>
#include <system_error>
#include <utility>

namespace threee::studio {

namespace {

std::uint64_t GetFileStamp(const std::filesystem::path& path) {
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);

    if (ec) {
        return 0;
    }

    return static_cast<std::uint64_t>(time.time_since_epoch().count());
}

std::filesystem::path ResolveProjectPath(
    const std::filesystem::path& projectRoot,
    const std::string& configuredPath) {

    if (configuredPath.empty()) {
        return {};
    }

    const std::filesystem::path path(configuredPath);

    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (projectRoot / path).lexically_normal();
}

std::string GetNestedString(
    const data::JsonValue& root,
    const char* objectName,
    const char* key,
    const std::string& defaultValue = {}) {

    const data::JsonValue* object = root.Find(objectName);

    if (!object || !object->IsObject()) {
        return defaultValue;
    }

    return object->GetString(key, defaultValue);
}

bool GetNestedBool(
    const data::JsonValue& root,
    const char* objectName,
    const char* key,
    bool defaultValue) {

    const data::JsonValue* object = root.Find(objectName);

    if (!object || !object->IsObject()) {
        return defaultValue;
    }

    return object->GetBool(key, defaultValue);
}

void AppendMissingPathError(
    std::string& error,
    const char* label,
    const std::filesystem::path& path) {

    if (!error.empty()) {
        error += "; ";
    }

    error += label;
    error += " not found: ";
    error += path.string();
}

std::string BuildRegistrySignature(
    const std::filesystem::path& projectsDirectory,
    const std::vector<ProjectDescriptor>& projects,
    const std::vector<ProjectDescriptor>& invalidProjects,
    const std::string& lastError) {

    std::ostringstream stream;

    stream << projectsDirectory.string() << '\n';
    stream << lastError << '\n';

    const auto append = [&stream](const ProjectDescriptor& project) {
        stream
            << project.id << '|'
            << project.displayName << '|'
            << project.gameId << '|'
            << project.manifestPath.string() << '|'
            << project.manifestStamp << '|'
            << project.gameRoot.string() << '|'
            << project.exportedAssets.string() << '|'
            << project.overlayPath.string() << '|'
            << project.cachePath.string() << '|'
            << project.tempPath.string() << '|'
            << (project.sourceReadOnly ? "1" : "0") << '|'
            << project.error
            << '\n';
    };

    for (const ProjectDescriptor& project : projects) {
        append(project);
    }

    stream << "--invalid--\n";

    for (const ProjectDescriptor& project : invalidProjects) {
        append(project);
    }

    return stream.str();
}

} // namespace

ProjectRegistry::ProjectRegistry(std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)),
      m_studioConfigPath(m_runtimeRoot / "config" / "studio.json"),
      m_projectsDirectory(m_runtimeRoot.parent_path() / "3E-Projects") {

    Update(true);
}

void ProjectRegistry::Update(bool force) {
    const auto now = std::chrono::steady_clock::now();

    if (!force && now < m_nextScan) {
        return;
    }

    m_nextScan = now + std::chrono::milliseconds(500);
    Scan();
}

void ProjectRegistry::Scan() {
    std::filesystem::path projectsDirectory =
        m_runtimeRoot.parent_path() / "3E-Projects";

    std::string lastError;

    if (std::filesystem::exists(m_studioConfigPath)) {
        const data::JsonDocument studioConfig =
            data::JsonDocument::LoadFile(m_studioConfigPath);

        if (!studioConfig.Ok()) {
            lastError =
                "Cannot parse config/studio.json: " +
                studioConfig.error;
        }
        else if (studioConfig.root.IsObject()) {
            const std::string configuredDirectory =
                studioConfig.root.GetString(
                    "projectsDirectory",
                    "../3E-Projects");

            if (!configuredDirectory.empty()) {
                const std::filesystem::path configuredPath(configuredDirectory);

                projectsDirectory =
                    configuredPath.is_absolute()
                        ? configuredPath
                        : (m_runtimeRoot / configuredPath).lexically_normal();
            }
        }
    }

    std::vector<ProjectDescriptor> projects;
    std::vector<ProjectDescriptor> invalidProjects;

    std::error_code ec;

    if (!std::filesystem::exists(projectsDirectory, ec)) {
        if (lastError.empty()) {
            lastError =
                "Projects directory not found: " +
                projectsDirectory.string();
        }
    }
    else {
        std::filesystem::directory_iterator iterator(projectsDirectory, ec);

        if (ec) {
            lastError =
                "Cannot enumerate projects directory: " +
                ec.message();
        }
        else {
            for (const auto& entry : iterator) {
                if (!entry.is_directory(ec)) {
                    ec.clear();
                    continue;
                }

                const std::filesystem::path projectRoot = entry.path();
                const std::filesystem::path manifestPath =
                    projectRoot / "project.3e.json";

                if (!std::filesystem::exists(manifestPath, ec)) {
                    ec.clear();
                    continue;
                }

                ProjectDescriptor project;
                project.projectRoot = projectRoot;
                project.manifestPath = manifestPath;
                project.manifestStamp = GetFileStamp(manifestPath);

                const data::JsonDocument manifest =
                    data::JsonDocument::LoadFile(manifestPath);

                if (!manifest.Ok()) {
                    project.displayName = projectRoot.filename().string();
                    project.error = "JSON parse error: " + manifest.error;
                    invalidProjects.push_back(std::move(project));
                    continue;
                }

                if (!manifest.root.IsObject()) {
                    project.displayName = projectRoot.filename().string();
                    project.error = "Root JSON value must be an object.";
                    invalidProjects.push_back(std::move(project));
                    continue;
                }

                project.id = manifest.root.GetString("id");
                project.displayName = manifest.root.GetString("displayName");
                project.gameId = manifest.root.GetString("game");

                project.gameRoot =
                    ResolveProjectPath(
                        projectRoot,
                        GetNestedString(manifest.root, "source", "gameRoot"));

                project.exportedAssets =
                    ResolveProjectPath(
                        projectRoot,
                        GetNestedString(manifest.root, "source", "exportedAssets"));

                project.sourceReadOnly =
                    GetNestedBool(manifest.root, "source", "readOnly", true);

                project.overlayPath =
                    ResolveProjectPath(
                        projectRoot,
                        GetNestedString(manifest.root, "workspace", "overlay", "overlay"));

                project.cachePath =
                    ResolveProjectPath(
                        projectRoot,
                        GetNestedString(manifest.root, "workspace", "cache", "cache"));

                project.tempPath =
                    ResolveProjectPath(
                        projectRoot,
                        GetNestedString(manifest.root, "workspace", "temp", "temp"));

                if (project.id.empty()) {
                    project.error = "Missing required field: id";
                }
                else if (project.displayName.empty()) {
                    project.error = "Missing required field: displayName";
                }
                else if (project.gameId.empty()) {
                    project.error = "Missing required field: game";
                }

                if (project.error.empty() && project.gameRoot.empty()) {
                    project.error = "Missing source.gameRoot";
                }

                if (project.error.empty() && project.exportedAssets.empty()) {
                    project.error = "Missing source.exportedAssets";
                }

                if (project.error.empty()) {
                    if (!std::filesystem::exists(project.gameRoot, ec)) {
                        ec.clear();
                        AppendMissingPathError(
                            project.error,
                            "Game root",
                            project.gameRoot);
                    }

                    if (!std::filesystem::exists(project.exportedAssets, ec)) {
                        ec.clear();
                        AppendMissingPathError(
                            project.error,
                            "ExportedAssets",
                            project.exportedAssets);
                    }

                    if (!std::filesystem::exists(project.overlayPath, ec)) {
                        ec.clear();
                        AppendMissingPathError(
                            project.error,
                            "Overlay",
                            project.overlayPath);
                    }

                    if (!std::filesystem::exists(project.cachePath, ec)) {
                        ec.clear();
                        AppendMissingPathError(
                            project.error,
                            "Cache",
                            project.cachePath);
                    }

                    if (!std::filesystem::exists(project.tempPath, ec)) {
                        ec.clear();
                        AppendMissingPathError(
                            project.error,
                            "Temp",
                            project.tempPath);
                    }
                }

                if (!project.error.empty()) {
                    invalidProjects.push_back(std::move(project));
                    continue;
                }

                projects.push_back(std::move(project));
            }
        }
    }

    const auto sorter = [](const ProjectDescriptor& a, const ProjectDescriptor& b) {
        if (a.displayName == b.displayName) {
            return a.id < b.id;
        }
        return a.displayName < b.displayName;
    };

    std::sort(projects.begin(), projects.end(), sorter);
    std::sort(invalidProjects.begin(), invalidProjects.end(), sorter);

    const std::string signature =
        BuildRegistrySignature(
            projectsDirectory,
            projects,
            invalidProjects,
            lastError);

    if (signature == m_signature) {
        return;
    }

    m_signature = signature;
    m_projectsDirectory = std::move(projectsDirectory);
    m_projects = std::move(projects);
    m_invalidProjects = std::move(invalidProjects);
    m_lastError = std::move(lastError);

    ++m_generation;
}

} // namespace threee::studio