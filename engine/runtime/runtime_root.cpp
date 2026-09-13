#include "engine/runtime/runtime_root.h"

#include <system_error>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace threee::runtime {

namespace {

bool LooksLikeRuntimeRoot(
    const std::filesystem::path& path) {

    std::error_code ec;

    const bool hasStudioConfig =
        std::filesystem::exists(
            path / "config" / "studio.json",
            ec);

    ec.clear();

    const bool hasGamesDirectory =
        std::filesystem::exists(
            path / "games",
            ec);

    return
        hasStudioConfig ||
        hasGamesDirectory;
}

bool LooksLikeSourceRoot(
    const std::filesystem::path& path) {

    std::error_code ec;

    return
        std::filesystem::exists(
            path / "premake5.lua",
            ec) &&
        std::filesystem::exists(
            path / "vendor" / "libp3d",
            ec);
}

std::filesystem::path SearchUpward(
    std::filesystem::path path) {

    std::error_code ec;

    path =
        std::filesystem::weakly_canonical(
            path,
            ec);

    if (ec) {
        ec.clear();

        path =
            std::filesystem::absolute(
                path,
                ec);
    }

    for (int depth = 0;
         depth < 10 && !path.empty();
         ++depth) {

        if (LooksLikeRuntimeRoot(path) ||
            LooksLikeSourceRoot(path)) {
            return path;
        }

        const std::filesystem::path parent =
            path.parent_path();

        if (parent == path) {
            break;
        }

        path = parent;
    }

    return {};
}

std::filesystem::path ExecutableDirectory(
    const char* executablePath) {

#if defined(_WIN32)
    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length > 0 &&
        length <
            static_cast<DWORD>(
                buffer.size())) {

        return
            std::filesystem::path(
                std::wstring(
                    buffer.data(),
                    length))
                .parent_path();
    }
#endif

    std::error_code ec;

    if (executablePath &&
        executablePath[0]) {

        std::filesystem::path executable(
            executablePath);

        if (executable.is_relative()) {
            executable =
                std::filesystem::absolute(
                    executable,
                    ec);
        }

        if (!ec) {
            return
                executable.parent_path();
        }
    }

    return {};
}

} // namespace

std::filesystem::path FindRuntimeRoot(
    const char* executablePath) {

    std::error_code ec;

    const std::filesystem::path executableDirectory =
        ExecutableDirectory(
            executablePath);

    if (!executableDirectory.empty() &&
        LooksLikeRuntimeRoot(
            executableDirectory)) {

        return executableDirectory;
    }

    const std::filesystem::path currentDirectory =
        std::filesystem::current_path(ec);

    if (!ec &&
        LooksLikeRuntimeRoot(
            currentDirectory)) {

        return currentDirectory;
    }

    if (!currentDirectory.empty()) {
        const auto found =
            SearchUpward(
                currentDirectory);

        if (!found.empty()) {
            return found;
        }
    }

    if (!executableDirectory.empty()) {
        const auto found =
            SearchUpward(
                executableDirectory);

        if (!found.empty()) {
            return found;
        }

        return executableDirectory;
    }

    return currentDirectory;
}

} // namespace threee::runtime