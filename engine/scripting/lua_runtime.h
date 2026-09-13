#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace threee::scripting {

struct ScriptContext {
    std::unordered_map<std::string, std::string> values;
};

struct ScriptResult {
    bool success = false;
    std::string message;
};

class LuaRuntime {
public:
    ScriptResult ExecuteFile(
        const std::filesystem::path& path,
        const std::string& entryFunction,
        const ScriptContext& context) const;
};

} // namespace threee::scripting