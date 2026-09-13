#include "apps/studio/action_executor.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif
#include <cstdint>
#include <ctime>
#include <functional>

namespace threee::studio {

namespace {

void ReplaceAll(
    std::string& value,
    const std::string& token,
    const std::string& replacement) {

    if (token.empty()) {
        return;
    }

    std::size_t position = 0;

    while ((position = value.find(token, position))
           != std::string::npos) {

        value.replace(
            position,
            token.size(),
            replacement);

        position += replacement.size();
    }
}

std::string QuoteArgument(
    const std::string& argument) {

    if (argument.empty()) {
        return "\"\"";
    }

    if (argument.find_first_of(" \t\"")
        == std::string::npos) {

        return argument;
    }

    std::string result = "\"";

    for (const char c : argument) {
        if (c == '"') {
            result += '\\';
        }

        result += c;
    }

    result += '"';
    return result;
}

std::string JoinArguments(
    const data::JsonValue* arguments,
    const std::function<std::string(const std::string&)>& expand) {

    if (!arguments || !arguments->IsArray()) {
        return {};
    }

    std::ostringstream stream;
    bool first = true;

    for (const data::JsonValue& value :
         arguments->arrayValue) {

        if (!value.IsString()) {
            continue;
        }

        if (!first) {
            stream << ' ';
        }

        stream << QuoteArgument(
            expand(value.stringValue));

        first = false;
    }

    return stream.str();
}

ActionResult Success(
    std::string message) {

    ActionResult result;
    result.success = true;
    result.message = std::move(message);
    return result;
}

ActionResult Failure(
    std::string message) {

    ActionResult result;
    result.success = false;
    result.message = std::move(message);
    return result;
}

} // namespace

ActionExecutor::ActionExecutor(
    std::filesystem::path runtimeRoot)
    : m_runtimeRoot(std::move(runtimeRoot)) {
}

std::string ActionExecutor::Timestamp() {
    const auto now =
        std::chrono::system_clock::now();

    const std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm local {};

#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif

    std::ostringstream stream;

    stream
        << std::put_time(
            &local,
            "%Y%m%d-%H%M%S");

    return stream.str();
}

std::string ActionExecutor::ExpandVariables(
    const std::string& value,
    const ActionContext& context) const {

    std::string result = value;

    ReplaceAll(
        result,
        "${RuntimeRoot}",
        context.runtimeRoot.string());

    ReplaceAll(
        result,
        "${ActiveGame}",
        context.activeGameId);

    ReplaceAll(
        result,
        "${ActiveProject}",
        context.activeProjectId);

    ReplaceAll(
        result,
        "${Timestamp}",
        Timestamp());

    if (context.project) {
        ReplaceAll(
            result,
            "${ProjectRoot}",
            context.project
                ->projectRoot
                .string());

        ReplaceAll(
            result,
            "${GameRoot}",
            context.project
                ->gameRoot
                .string());

        ReplaceAll(
            result,
            "${ExportedAssets}",
            context.project
                ->exportedAssets
                .string());

        ReplaceAll(
            result,
            "${Overlay}",
            context.project
                ->overlayPath
                .string());

        ReplaceAll(
            result,
            "${Cache}",
            context.project
                ->cachePath
                .string());

        ReplaceAll(
            result,
            "${Temp}",
            context.project
                ->tempPath
                .string());
    }

    return result;
}

std::filesystem::path ActionExecutor::ResolvePath(
    const std::string& value,
    const ActionContext& context) const {

    if (value.empty()) {
        return {};
    }

    std::filesystem::path path(
        ExpandVariables(
            value,
            context));

    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (
        context.runtimeRoot /
        path).lexically_normal();
}

ActionResult ActionExecutor::Execute(
    const CommandDescriptor& command,
    const ActionContext& context) const {

    if (!command.action.IsObject()) {
        return Failure(
            "Command has no valid action: " +
            command.id);
    }

    return ExecuteAction(
        command.action,
        context);
}

ActionResult ActionExecutor::ExecuteAction(
    const data::JsonValue& action,
    const ActionContext& context) const {

    if (!action.IsObject()) {
        return Failure(
            "Action must be a JSON object.");
    }

    const std::string type =
        action.GetString("type");

    if (type.empty()) {
        return Failure(
            "Action type is missing.");
    }

    if (type == "sequence") {
        const data::JsonValue* steps =
            action.Find("steps");

        if (!steps || !steps->IsArray()) {
            return Failure(
                "Sequence action has no steps.");
        }

        int completed = 0;

        for (const data::JsonValue& step :
             steps->arrayValue) {

            const ActionResult result =
                ExecuteAction(
                    step,
                    context);

            if (!result.success) {
                return Failure(
                    "Sequence stopped after " +
                    std::to_string(completed) +
                    " step(s): " +
                    result.message);
            }

            ++completed;
        }

        return Success(
            "Sequence completed: " +
            std::to_string(completed) +
            " step(s).");
    }

    if (type == "validate") {
        const data::JsonValue* paths =
            action.Find("paths");

        if (!paths || !paths->IsArray()) {
            return Failure(
                "Validate action requires paths[].");
        }

        std::vector<std::string> missing;

        for (const data::JsonValue& value :
             paths->arrayValue) {

            if (!value.IsString()) {
                continue;
            }

            const std::filesystem::path path =
                ResolvePath(
                    value.stringValue,
                    context);

            std::error_code ec;

            if (path.empty() ||
                !std::filesystem::exists(
                    path,
                    ec)) {

                missing.push_back(
                    path.empty()
                        ? value.stringValue
                        : path.string());
            }
        }

        if (!missing.empty()) {
            std::ostringstream message;

            message
                << "Validation failed. Missing: ";

            for (std::size_t i = 0;
                 i < missing.size();
                 ++i) {

                if (i > 0) {
                    message << "; ";
                }

                message << missing[i];
            }

            return Failure(
                message.str());
        }

        return Success(
            "Validation passed.");
    }

    if (type == "mkdir") {
        const std::filesystem::path path =
            ResolvePath(
                action.GetString("path"),
                context);

        if (path.empty()) {
            return Failure(
                "mkdir action requires path.");
        }

        std::error_code ec;

        std::filesystem::create_directories(
            path,
            ec);

        if (ec) {
            return Failure(
                "Cannot create directory: " +
                path.string() +
                " (" +
                ec.message() +
                ")");
        }

        return Success(
            "Directory ready: " +
            path.string());
    }

    if (type == "open") {
        const std::filesystem::path path =
            ResolvePath(
                action.GetString("path"),
                context);

        if (path.empty()) {
            return Failure(
                "open action requires path.");
        }

        std::error_code ec;

        if (!std::filesystem::exists(path, ec)) {
            return Failure(
                "Path does not exist: " +
                path.string());
        }

#if defined(_WIN32)
        const HINSTANCE result =
            ShellExecuteW(
                nullptr,
                L"open",
                path.wstring().c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);

        if (reinterpret_cast<std::intptr_t>(result) <= 32) {
            return Failure(
                "Windows could not open: " +
                path.string());
        }
#else
        const std::string command =
            "xdg-open " +
            QuoteArgument(path.string()) +
            " >/dev/null 2>&1 &";

        if (std::system(command.c_str()) != 0) {
            return Failure(
                "Could not open: " +
                path.string());
        }
#endif

        return Success(
            "Opened: " +
            path.string());
    }

    if (type == "process") {
        const std::string programText =
            ExpandVariables(
                action.GetString("program"),
                context);

        if (programText.empty()) {
            return Failure(
                "process action requires program.");
        }

        const std::string arguments =
            JoinArguments(
                action.Find("arguments"),
                [this, &context](const std::string& value) {
                    return ExpandVariables(
                        value,
                        context);
                });

        const std::string workingDirectoryText =
            action.GetString(
                "workingDirectory");

        const std::filesystem::path workingDirectory =
            workingDirectoryText.empty()
                ? context.runtimeRoot
                : ResolvePath(
                    workingDirectoryText,
                    context);

#if defined(_WIN32)
        std::filesystem::path programPath(
            programText);

        const HINSTANCE result =
            ShellExecuteW(
                nullptr,
                L"open",
                programPath.wstring().c_str(),
                std::filesystem::path(arguments).wstring().c_str(),
                workingDirectory.wstring().c_str(),
                SW_SHOWNORMAL);

        if (reinterpret_cast<std::intptr_t>(result) <= 32) {
            return Failure(
                "Could not launch process: " +
                programText);
        }
#else
        const std::string command =
            "cd " +
            QuoteArgument(
                workingDirectory.string()) +
            " && " +
            QuoteArgument(programText) +
            (arguments.empty()
                 ? ""
                 : " " + arguments) +
            " &";

        if (std::system(command.c_str()) != 0) {
            return Failure(
                "Could not launch process: " +
                programText);
        }
#endif

        return Success(
            "Process launched: " +
            programText);
    }

    if (type == "copy" ||
        type == "backup") {

        const std::filesystem::path source =
            ResolvePath(
                action.GetString("source"),
                context);

        const std::filesystem::path destination =
            ResolvePath(
                action.GetString("destination"),
                context);

        if (source.empty() ||
            destination.empty()) {

            return Failure(
                type +
                " action requires source and destination.");
        }

        std::error_code ec;

        if (!std::filesystem::exists(source, ec)) {
            return Failure(
                "Source does not exist: " +
                source.string());
        }

        std::filesystem::create_directories(
            destination.parent_path(),
            ec);

        if (ec) {
            return Failure(
                "Cannot prepare destination: " +
                destination.string() +
                " (" +
                ec.message() +
                ")");
        }

        ec.clear();

        if (std::filesystem::is_directory(
                source,
                ec)) {

            ec.clear();

            std::filesystem::copy(
                source,
                destination,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
                ec);
        }
        else {
            ec.clear();

            std::filesystem::copy_file(
                source,
                destination,
                std::filesystem::copy_options::overwrite_existing,
                ec);
        }

        if (ec) {
            return Failure(
                "Copy failed: " +
                ec.message());
        }

        return Success(
            (type == "backup"
                 ? "Backup created: "
                 : "Copied to: ") +
            destination.string());
    }

    if (type == "move") {
        const std::filesystem::path source =
            ResolvePath(
                action.GetString("source"),
                context);

        const std::filesystem::path destination =
            ResolvePath(
                action.GetString("destination"),
                context);

        if (source.empty() ||
            destination.empty()) {

            return Failure(
                "move action requires source and destination.");
        }

        std::error_code ec;

        if (!std::filesystem::exists(source, ec)) {
            return Failure(
                "Source does not exist: " +
                source.string());
        }

        std::filesystem::create_directories(
            destination.parent_path(),
            ec);

        if (ec) {
            return Failure(
                "Cannot prepare destination: " +
                ec.message());
        }

        ec.clear();

        if (std::filesystem::exists(destination, ec)) {
            ec.clear();

            std::filesystem::remove_all(
                destination,
                ec);

            if (ec) {
                return Failure(
                    "Cannot replace destination: " +
                    ec.message());
            }
        }

        ec.clear();

        std::filesystem::rename(
            source,
            destination,
            ec);

        if (ec) {
            return Failure(
                "Move failed: " +
                ec.message());
        }

        return Success(
            "Moved to: " +
            destination.string());
    }

    return Failure(
        "Unsupported action type: " +
        type);
}

} // namespace threee::studio