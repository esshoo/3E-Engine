#pragma once

#include <filesystem>

namespace threee::runtime {

std::filesystem::path FindRuntimeRoot(
    const char* executablePath);

} // namespace threee::runtime