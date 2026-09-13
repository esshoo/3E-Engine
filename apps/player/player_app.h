#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace threee::player {

struct PlayerLaunchRequest {
    std::string gameId;
    std::filesystem::path projectManifest;
};

struct PlayerRuntimeState {
    bool valid = false;

    std::filesystem::path runtimeRoot;

    std::string gameId;
    std::string gameDisplayName;
    std::filesystem::path gameManifest;

    std::string runtimeHost;
    std::string runtimeMode;
    std::string runtimeExecutableSpec;
    std::string runtimeWorkingDirectorySpec;
    std::vector<std::string> runtimeArguments;

    std::string projectId;
    std::string projectDisplayName;
    std::filesystem::path projectManifest;
    std::filesystem::path projectRoot;

    std::filesystem::path sourceGameRoot;
    std::filesystem::path exportedAssets;
    std::filesystem::path overlayPath;

    std::filesystem::path resolvedRuntimeExecutable;
    std::filesystem::path resolvedRuntimeWorkingDirectory;

    std::string error;

    bool Load(
        std::filesystem::path inRuntimeRoot,
        const PlayerLaunchRequest& request);

    bool ResolveRuntimeLaunchTarget();
};

int LaunchRuntimeAndWait(
    const PlayerRuntimeState& state,
    std::string& error);

bool WriteVerificationReport(
    const PlayerRuntimeState& state,
    const std::filesystem::path& reportPath);

} // namespace threee::player
