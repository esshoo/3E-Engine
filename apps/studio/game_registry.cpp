#include "apps/studio/game_registry.h"

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

std::string GetIntegrationState(const data::JsonValue& root) {
    const data::JsonValue* integration = root.Find("integration");

    if (!integration || !integration->IsObject()) {
        return {};
    }

    return integration->GetString("state");
}

std::string BuildRegistrySignature(
    const std::filesystem::path& gamesDirectory,
    const std::vector<GameDescriptor>& games,
    const std::vector<GameDescriptor>& invalidGames,
    const std::string& lastError) {

    std::ostringstream stream;
    stream << gamesDirectory.string() << '\n';
    stream << lastError << '\n';

    const auto append = [&stream](const GameDescriptor& game) {
        stream
            << game.id << '|'
            << game.displayName << '|'
            << game.integrationState << '|'
            << game.rootPath.string() << '|'
            << game.manifestPath.string() << '|'
            << game.manifestStamp << '|'
            << (game.enabled ? "1" : "0") << '|'
            << game.error
            << '\n';
    };

    for (const GameDescriptor& game : games) {
        append(game);
    }

    stream << "--invalid--\n";

    for (const GameDescriptor& game : invalidGames) {
        append(game);
    }

    return stream.str();
}

} // namespace

GameRegistry::GameRegistry(std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)),
      m_studioConfigPath(m_runtimeRoot / "config" / "studio.json"),
      m_gamesDirectory(m_runtimeRoot / "games") {

    Update(true);
}

void GameRegistry::Update(bool force) {
    const auto now = std::chrono::steady_clock::now();

    if (!force && now < m_nextScan) {
        return;
    }

    m_nextScan = now + std::chrono::milliseconds(500);
    Scan();
}

void GameRegistry::Scan() {
    std::filesystem::path gamesDirectory = m_runtimeRoot / "games";
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
                studioConfig.root.GetString("gamesDirectory", "games");

            if (!configuredDirectory.empty()) {
                std::filesystem::path configuredPath(configuredDirectory);

                gamesDirectory = configuredPath.is_absolute()
                    ? configuredPath
                    : m_runtimeRoot / configuredPath;
            }
        }
    }

    std::vector<GameDescriptor> games;
    std::vector<GameDescriptor> invalidGames;

    std::error_code ec;

    if (!std::filesystem::exists(gamesDirectory, ec)) {
        if (lastError.empty()) {
            lastError =
                "Games directory not found: " +
                gamesDirectory.string();
        }
    }
    else {
        std::filesystem::directory_iterator iterator(gamesDirectory, ec);

        if (ec) {
            lastError =
                "Cannot enumerate games directory: " +
                ec.message();
        }
        else {
            for (const auto& entry : iterator) {
                if (!entry.is_directory(ec)) {
                    ec.clear();
                    continue;
                }

                const std::filesystem::path gameRoot = entry.path();
                const std::filesystem::path manifestPath = gameRoot / "game.json";

                if (!std::filesystem::exists(manifestPath, ec)) {
                    ec.clear();
                    continue;
                }

                GameDescriptor game;
                game.rootPath = gameRoot;
                game.manifestPath = manifestPath;
                game.manifestStamp = GetFileStamp(manifestPath);

                const data::JsonDocument manifest =
                    data::JsonDocument::LoadFile(manifestPath);

                if (!manifest.Ok()) {
                    game.error = "JSON parse error: " + manifest.error;
                    game.displayName = gameRoot.filename().string();
                    invalidGames.push_back(std::move(game));
                    continue;
                }

                if (!manifest.root.IsObject()) {
                    game.error = "Root JSON value must be an object.";
                    game.displayName = gameRoot.filename().string();
                    invalidGames.push_back(std::move(game));
                    continue;
                }

                game.id = manifest.root.GetString("id");
                game.displayName = manifest.root.GetString("displayName");
                game.enabled = manifest.root.GetBool("enabled", true);
                game.integrationState = GetIntegrationState(manifest.root);

                if (game.id == "__template__" || !game.enabled) {
                    continue;
                }

                if (game.id.empty()) {
                    game.error = "Missing required field: id";
                }
                else if (game.displayName.empty()) {
                    game.error = "Missing required field: displayName";
                }

                if (!game.error.empty()) {
                    invalidGames.push_back(std::move(game));
                    continue;
                }

                games.push_back(std::move(game));
            }
        }
    }

    const auto sorter = [](const GameDescriptor& a, const GameDescriptor& b) {
        if (a.displayName == b.displayName) {
            return a.id < b.id;
        }
        return a.displayName < b.displayName;
    };

    std::sort(games.begin(), games.end(), sorter);
    std::sort(invalidGames.begin(), invalidGames.end(), sorter);

    const std::string signature =
        BuildRegistrySignature(gamesDirectory, games, invalidGames, lastError);

    if (signature == m_signature) {
        return;
    }

    m_signature = signature;
    m_gamesDirectory = std::move(gamesDirectory);
    m_games = std::move(games);
    m_invalidGames = std::move(invalidGames);
    m_lastError = std::move(lastError);

    ++m_generation;
}

} // namespace threee::studio