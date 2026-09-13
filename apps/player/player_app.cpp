#include "apps/player/player_app.h"

#include "engine/data/json_value.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace threee::player {

namespace {

std::filesystem::path ResolveProjectPath(
    const std::filesystem::path& projectRoot,
    const std::string& value) {

    if (value.empty()) {
        return {};
    }

    std::filesystem::path path(value);

    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (projectRoot / path).lexically_normal();
}

std::string NestedString(
    const data::JsonValue& root,
    const char* objectName,
    const char* key) {

    const data::JsonValue* object =
        root.Find(objectName);

    if (!object || !object->IsObject()) {
        return {};
    }

    return object->GetString(key);
}

bool Exists(
    const std::filesystem::path& path) {

    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void ReplaceAll(
    std::string& value,
    const std::string& token,
    const std::string& replacement) {

    if (token.empty()) {
        return;
    }

    std::size_t position = 0;

    while ((position = value.find(token, position)) !=
           std::string::npos) {

        value.replace(
            position,
            token.size(),
            replacement);

        position += replacement.size();
    }
}

std::string ExpandRuntimeValue(
    std::string value,
    const PlayerRuntimeState& state) {

    ReplaceAll(
        value,
        "${RuntimeRoot}",
        state.runtimeRoot.string());

    ReplaceAll(
        value,
        "${ProjectRoot}",
        state.projectRoot.string());

    ReplaceAll(
        value,
        "${GameRoot}",
        state.sourceGameRoot.string());

    ReplaceAll(
        value,
        "${ExportedAssets}",
        state.exportedAssets.string());

    ReplaceAll(
        value,
        "${Overlay}",
        state.overlayPath.string());

    ReplaceAll(
        value,
        "${GameId}",
        state.gameId);

    ReplaceAll(
        value,
        "${ProjectId}",
        state.projectId);

    return value;
}

bool HasUnresolvedVariable(
    const std::string& value) {

    return value.find("${") != std::string::npos;
}

std::vector<std::string> ReadStringArray(
    const data::JsonValue* value) {

    std::vector<std::string> result;

    if (!value || !value->IsArray()) {
        return result;
    }

    for (const data::JsonValue& item :
         value->arrayValue) {

        if (item.IsString()) {
            result.push_back(item.stringValue);
        }
    }

    return result;
}

#if defined(_WIN32)
std::wstring QuoteWindowsArgument(
    const std::wstring& argument) {

    if (argument.empty()) {
        return L"\"\"";
    }

    if (argument.find_first_of(L" \t\"") ==
        std::wstring::npos) {

        return argument;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;

    for (const wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }

        if (c == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'\"');
            backslashes = 0;
            continue;
        }

        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(c);
    }

    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');

    return result;
}
#endif

} // namespace

bool PlayerRuntimeState::Load(
    std::filesystem::path inRuntimeRoot,
    const PlayerLaunchRequest& request) {

    *this = {};

    runtimeRoot =
        std::move(inRuntimeRoot);

    gameId =
        request.gameId;

    if (!request.projectManifest.empty()) {
        projectManifest =
            request.projectManifest;

        if (projectManifest.is_relative()) {
            std::error_code ec;

            projectManifest =
                std::filesystem::absolute(
                    projectManifest,
                    ec);

            if (ec) {
                error =
                    "Cannot resolve project path: " +
                    ec.message();

                return false;
            }
        }

        projectManifest =
            projectManifest.lexically_normal();

        if (!Exists(projectManifest)) {
            error =
                "Project manifest not found: " +
                projectManifest.string();

            return false;
        }

        const data::JsonDocument project =
            data::JsonDocument::LoadFile(
                projectManifest);

        if (!project.Ok()) {
            error =
                "Cannot parse project manifest: " +
                project.error;

            return false;
        }

        if (!project.root.IsObject()) {
            error =
                "Project manifest root must be an object.";

            return false;
        }

        projectRoot =
            projectManifest.parent_path();

        projectId =
            project.root.GetString("id");

        projectDisplayName =
            project.root.GetString(
                "displayName",
                projectId);

        const std::string projectGame =
            project.root.GetString("game");

        if (gameId.empty()) {
            gameId = projectGame;
        }
        else if (!projectGame.empty() &&
                 projectGame != gameId) {

            error =
                "Project game mismatch. Requested '" +
                gameId +
                "' but project uses '" +
                projectGame +
                "'.";

            return false;
        }

        sourceGameRoot =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "source",
                    "gameRoot"));

        exportedAssets =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "source",
                    "exportedAssets"));

        overlayPath =
            ResolveProjectPath(
                projectRoot,
                NestedString(
                    project.root,
                    "workspace",
                    "overlay"));

        if (sourceGameRoot.empty()) {
            error =
                "Project is missing source.gameRoot.";

            return false;
        }

        if (exportedAssets.empty()) {
            error =
                "Project is missing source.exportedAssets.";

            return false;
        }

        if (overlayPath.empty()) {
            error =
                "Project is missing workspace.overlay.";

            return false;
        }

        if (!Exists(sourceGameRoot)) {
            error =
                "Project GameRoot not found: " +
                sourceGameRoot.string();

            return false;
        }

        if (!Exists(exportedAssets)) {
            error =
                "Project ExportedAssets not found: " +
                exportedAssets.string();

            return false;
        }

        if (!Exists(overlayPath)) {
            error =
                "Project Overlay not found: " +
                overlayPath.string();

            return false;
        }
    }

    if (gameId.empty()) {
        error =
            "No game was selected. Use --game <id> or --project <manifest>.";

        return false;
    }

    gameManifest =
        runtimeRoot /
        "games" /
        gameId /
        "game.json";

    if (!Exists(gameManifest)) {
        error =
            "Game manifest not found: " +
            gameManifest.string();

        return false;
    }

    const data::JsonDocument game =
        data::JsonDocument::LoadFile(
            gameManifest);

    if (!game.Ok()) {
        error =
            "Cannot parse game manifest: " +
            game.error;

        return false;
    }

    if (!game.root.IsObject()) {
        error =
            "Game manifest root must be an object.";

        return false;
    }

    const std::string manifestGameId =
        game.root.GetString("id");

    if (manifestGameId.empty()) {
        error =
            "Game manifest is missing id.";

        return false;
    }

    if (manifestGameId != gameId) {
        error =
            "Game manifest id mismatch. Folder requested '" +
            gameId +
            "', manifest declares '" +
            manifestGameId +
            "'.";

        return false;
    }

    if (!game.root.GetBool(
            "enabled",
            true)) {

        error =
            "Game is disabled: " +
            gameId;

        return false;
    }

    gameDisplayName =
        game.root.GetString(
            "displayName",
            gameId);

    const data::JsonValue* runtime =
        game.root.Find("runtime");

    if (!runtime || !runtime->IsObject()) {
        error =
            "Game manifest is missing runtime configuration.";

        return false;
    }

    runtimeHost =
        runtime->GetString("host");

    runtimeMode =
        runtime->GetString("mode");

    runtimeExecutableSpec =
        runtime->GetString("executable");

    runtimeWorkingDirectorySpec =
        runtime->GetString(
            "workingDirectory",
            "${GameRoot}");

    runtimeArguments =
        ReadStringArray(
            runtime->Find("arguments"));

    if (!runtimeHost.empty() &&
        runtimeHost != "3E-Player") {

        error =
            "Game runtime host is not 3E-Player: " +
            runtimeHost;

        return false;
    }

    if (runtimeMode != "legacy-bridge") {
        error =
            "Unsupported runtime mode: " +
            runtimeMode;

        return false;
    }

    if (runtimeExecutableSpec.empty()) {
        error =
            "Legacy bridge is missing runtime.executable.";

        return false;
    }

    if (!projectManifest.empty() &&
        !ResolveRuntimeLaunchTarget()) {

        return false;
    }

    valid = true;
    error.clear();

    return true;
}

bool PlayerRuntimeState::ResolveRuntimeLaunchTarget() {
    const std::string expandedExecutable =
        ExpandRuntimeValue(
            runtimeExecutableSpec,
            *this);

    if (expandedExecutable.empty() ||
        HasUnresolvedVariable(expandedExecutable)) {

        error =
            "Runtime executable could not be resolved: " +
            runtimeExecutableSpec;

        return false;
    }

    resolvedRuntimeExecutable =
        std::filesystem::path(
            expandedExecutable);

    if (resolvedRuntimeExecutable.is_relative()) {
        const std::filesystem::path base =
            sourceGameRoot.empty()
                ? runtimeRoot
                : sourceGameRoot;

        resolvedRuntimeExecutable =
            (base / resolvedRuntimeExecutable)
                .lexically_normal();
    }
    else {
        resolvedRuntimeExecutable =
            resolvedRuntimeExecutable
                .lexically_normal();
    }

    const std::string expandedWorkingDirectory =
        ExpandRuntimeValue(
            runtimeWorkingDirectorySpec,
            *this);

    if (expandedWorkingDirectory.empty() ||
        HasUnresolvedVariable(
            expandedWorkingDirectory)) {

        resolvedRuntimeWorkingDirectory =
            sourceGameRoot;
    }
    else {
        resolvedRuntimeWorkingDirectory =
            std::filesystem::path(
                expandedWorkingDirectory);

        if (resolvedRuntimeWorkingDirectory.is_relative()) {
            const std::filesystem::path base =
                sourceGameRoot.empty()
                    ? runtimeRoot
                    : sourceGameRoot;

            resolvedRuntimeWorkingDirectory =
                (base / resolvedRuntimeWorkingDirectory)
                    .lexically_normal();
        }
        else {
            resolvedRuntimeWorkingDirectory =
                resolvedRuntimeWorkingDirectory
                    .lexically_normal();
        }
    }

    if (!Exists(resolvedRuntimeExecutable)) {
        error =
            "Runtime executable not found: " +
            resolvedRuntimeExecutable.string();

        return false;
    }

    if (resolvedRuntimeWorkingDirectory.empty() ||
        !Exists(resolvedRuntimeWorkingDirectory)) {

        error =
            "Runtime working directory not found: " +
            resolvedRuntimeWorkingDirectory.string();

        return false;
    }

    return true;
}

int LaunchRuntimeAndWait(
    const PlayerRuntimeState& state,
    std::string& error) {

    if (!state.valid) {
        error =
            state.error.empty()
                ? "Runtime state is invalid."
                : state.error;

        return 2;
    }

    if (state.projectManifest.empty()) {
        error =
            "A project manifest is required to launch this runtime.";

        return 3;
    }

    if (state.resolvedRuntimeExecutable.empty()) {
        error =
            "Runtime executable is not resolved.";

        return 4;
    }

#if defined(_WIN32)
    std::wstring commandLine =
        QuoteWindowsArgument(
            state.resolvedRuntimeExecutable
                .wstring());

    for (const std::string& rawArgument :
         state.runtimeArguments) {

        const std::string expanded =
            ExpandRuntimeValue(
                rawArgument,
                state);

        commandLine += L" ";
        commandLine +=
            QuoteWindowsArgument(
                std::filesystem::path(
                    expanded)
                    .wstring());
    }

    std::vector<wchar_t> mutableCommand(
        commandLine.begin(),
        commandLine.end());

    mutableCommand.push_back(L'\0');

    HANDLE job =
        CreateJobObjectW(
            nullptr,
            nullptr);

    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};

        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(
                job,
                JobObjectExtendedLimitInformation,
                &info,
                sizeof(info))) {

            CloseHandle(job);
            job = nullptr;
        }
    }

    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process {};

    const BOOL created =
        CreateProcessW(
            state.resolvedRuntimeExecutable
                .wstring()
                .c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            state.resolvedRuntimeWorkingDirectory
                .wstring()
                .c_str(),
            &startup,
            &process);

    if (!created) {
        if (job) {
            CloseHandle(job);
        }

        error =
            "CreateProcessW failed for " +
            state.resolvedRuntimeExecutable.string() +
            ". Win32 error: " +
            std::to_string(GetLastError());

        return 5;
    }

    if (job) {
        if (!AssignProcessToJobObject(
                job,
                process.hProcess)) {

            CloseHandle(job);
            job = nullptr;
        }
    }

    CloseHandle(process.hThread);

    const DWORD waitResult =
        WaitForSingleObject(
            process.hProcess,
            INFINITE);

    DWORD exitCode = 0;

    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(
            process.hProcess,
            &exitCode);
    }
    else {
        error =
            "Waiting for runtime process failed. Win32 error: " +
            std::to_string(GetLastError());

        exitCode = 6;
    }

    CloseHandle(process.hProcess);

    if (job) {
        CloseHandle(job);
    }

    return static_cast<int>(exitCode);
#else
    std::error_code ec;
    const std::filesystem::path previousDirectory =
        std::filesystem::current_path(ec);

    ec.clear();
    std::filesystem::current_path(
        state.resolvedRuntimeWorkingDirectory,
        ec);

    if (ec) {
        error =
            "Cannot enter runtime working directory: " +
            ec.message();

        return 7;
    }

    std::ostringstream command;
    command
        << '"'
        << state.resolvedRuntimeExecutable.string()
        << '"';

    for (const std::string& rawArgument :
         state.runtimeArguments) {

        command
            << " \""
            << ExpandRuntimeValue(
                   rawArgument,
                   state)
            << '"';
    }

    const int result =
        std::system(
            command.str().c_str());

    if (!previousDirectory.empty()) {
        ec.clear();
        std::filesystem::current_path(
            previousDirectory,
            ec);
    }

    return result;
#endif
}

bool WriteVerificationReport(
    const PlayerRuntimeState& state,
    const std::filesystem::path& reportPath) {

    std::ofstream report(
        reportPath,
        std::ios::binary);

    if (!report) {
        return false;
    }

    report
        << "3E Player Runtime Verification\n"
        << "==============================\n\n"
        << "Valid="
        << (state.valid ? "true" : "false")
        << "\n"
        << "RuntimeRoot="
        << state.runtimeRoot.string()
        << "\n"
        << "GameId="
        << state.gameId
        << "\n"
        << "GameDisplayName="
        << state.gameDisplayName
        << "\n"
        << "GameManifest="
        << state.gameManifest.string()
        << "\n"
        << "RuntimeHost="
        << state.runtimeHost
        << "\n"
        << "RuntimeMode="
        << state.runtimeMode
        << "\n"
        << "RuntimeExecutableSpec="
        << state.runtimeExecutableSpec
        << "\n"
        << "ResolvedRuntimeExecutable="
        << state.resolvedRuntimeExecutable.string()
        << "\n"
        << "RuntimeWorkingDirectory="
        << state.resolvedRuntimeWorkingDirectory.string()
        << "\n"
        << "ProjectId="
        << state.projectId
        << "\n"
        << "ProjectManifest="
        << state.projectManifest.string()
        << "\n"
        << "GameRoot="
        << state.sourceGameRoot.string()
        << "\n"
        << "ExportedAssets="
        << state.exportedAssets.string()
        << "\n"
        << "Overlay="
        << state.overlayPath.string()
        << "\n"
        << "Error="
        << state.error
        << "\n";

    return true;
}

} // namespace threee::player
