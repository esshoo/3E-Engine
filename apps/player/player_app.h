#pragma once

#include <filesystem>
#include <string>

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

    std::string projectId;
    std::string projectDisplayName;
    std::filesystem::path projectManifest;
    std::filesystem::path projectRoot;

    std::filesystem::path sourceGameRoot;
    std::filesystem::path exportedAssets;
    std::filesystem::path overlayPath;

    std::string error;

    bool Load(
        std::filesystem::path inRuntimeRoot,
        const PlayerLaunchRequest& request);
};

class PlayerApp {
public:
    explicit PlayerApp(
        PlayerRuntimeState state);

    void Update();
    void Draw();

private:
    PlayerRuntimeState m_state;
};

bool WriteVerificationReport(
    const PlayerRuntimeState& state,
    const std::filesystem::path& reportPath);

} // namespace threee::player