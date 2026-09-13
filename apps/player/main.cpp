#include "apps/player/player_app.h"
#include "engine/runtime/runtime_root.h"

#include <filesystem>
#include <string>

namespace {

bool ParseArguments(
    int argc,
    char** argv,
    threee::player::PlayerLaunchRequest& request,
    bool& verifyRuntime,
    std::string& error) {

    verifyRuntime = false;

    for (int i = 1;
         i < argc;
         ++i) {

        const char* raw =
            argv[i];

        if (!raw) {
            continue;
        }

        const std::string argument(raw);

        if (argument == "--verify-runtime") {
            verifyRuntime = true;
            continue;
        }

        if (argument == "--game") {
            if (i + 1 >= argc ||
                !argv[i + 1]) {

                error =
                    "--game requires a value.";

                return false;
            }

            request.gameId =
                argv[++i];

            continue;
        }

        if (argument == "--project") {
            if (i + 1 >= argc ||
                !argv[i + 1]) {

                error =
                    "--project requires a path.";

                return false;
            }

            request.projectManifest =
                std::filesystem::path(
                    argv[++i]);

            continue;
        }

        if (argument == "--help" ||
            argument == "-h") {

            error =
                "Usage: 3E-Player --game <id> --project <project.3e.json> [--verify-runtime]";

            return false;
        }

        error =
            "Unknown argument: " +
            argument;

        return false;
    }

    return true;
}

} // namespace

int main(
    int argc,
    char** argv) {

    const std::filesystem::path runtimeRoot =
        threee::runtime::FindRuntimeRoot(
            argv && argv[0]
                ? argv[0]
                : nullptr);

    threee::player::PlayerLaunchRequest request;
    bool verifyRuntime = false;
    std::string argumentError;

    const bool argumentsOk =
        ParseArguments(
            argc,
            argv,
            request,
            verifyRuntime,
            argumentError);

    threee::player::PlayerRuntimeState state;

    if (argumentsOk) {
        state.Load(
            runtimeRoot,
            request);
    }
    else {
        state.runtimeRoot =
            runtimeRoot;

        state.error =
            argumentError;
    }

    const std::filesystem::path verificationPath =
        runtimeRoot /
        "3E-Player-Verify.txt";

    if (verifyRuntime) {
        threee::player::WriteVerificationReport(
            state,
            verificationPath);

        return
            state.valid
                ? 0
                : 2;
    }

    if (!state.valid) {
        threee::player::WriteVerificationReport(
            state,
            runtimeRoot /
                "3E-Player-Last.txt");

        return 2;
    }

    std::string launchError;

    const int exitCode =
        threee::player::LaunchRuntimeAndWait(
            state,
            launchError);

    if (!launchError.empty()) {
        state.error =
            launchError;

        threee::player::WriteVerificationReport(
            state,
            runtimeRoot /
                "3E-Player-Last.txt");
    }

    return exitCode;
}
