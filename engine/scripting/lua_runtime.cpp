#include "engine/scripting/lua_runtime.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

namespace threee::scripting {

namespace {

std::string LuaError(lua_State* state) {
    const char* error =
        lua_tostring(state, -1);

    return error
        ? error
        : "Unknown Lua error.";
}

std::string* LogTarget(lua_State* state) {
    return static_cast<std::string*>(
        lua_touserdata(
            state,
            lua_upvalueindex(1)));
}

int LuaLog(lua_State* state) {
    std::string* target =
        LogTarget(state);

    if (!target) {
        return 0;
    }

    const int count =
        lua_gettop(state);

    for (int i = 1; i <= count; ++i) {
        size_t length = 0;

        const char* text =
            luaL_tolstring(
                state,
                i,
                &length);

        if (text) {
            if (!target->empty()) {
                target->append("\n");
            }

            target->append(
                text,
                length);
        }

        lua_pop(state, 1);
    }

    return 0;
}

int LuaExists(lua_State* state) {
    const char* raw =
        luaL_checkstring(
            state,
            1);

    std::error_code ec;

    const bool exists =
        raw &&
        std::filesystem::exists(
            std::filesystem::path(raw),
            ec);

    lua_pushboolean(
        state,
        exists ? 1 : 0);

    return 1;
}

int LuaMkdir(lua_State* state) {
    const char* raw =
        luaL_checkstring(
            state,
            1);

    if (!raw || !raw[0]) {
        lua_pushboolean(state, 0);
        return 1;
    }

    std::error_code ec;

    std::filesystem::create_directories(
        std::filesystem::path(raw),
        ec);

    lua_pushboolean(
        state,
        ec ? 0 : 1);

    if (ec) {
        lua_pushstring(
            state,
            ec.message().c_str());

        return 2;
    }

    return 1;
}

void PushContext(
    lua_State* state,
    const ScriptContext& context) {

    lua_newtable(state);

    for (const auto& [key, value] :
         context.values) {

        lua_pushlstring(
            state,
            value.data(),
            value.size());

        lua_setfield(
            state,
            -2,
            key.c_str());
    }
}

void InstallThreeETable(
    lua_State* state,
    std::string& log) {

    lua_newtable(state);

    lua_pushlightuserdata(
        state,
        &log);

    lua_pushcclosure(
        state,
        LuaLog,
        1);

    lua_setfield(
        state,
        -2,
        "log");

    lua_pushcfunction(
        state,
        LuaExists);

    lua_setfield(
        state,
        -2,
        "exists");

    lua_pushcfunction(
        state,
        LuaMkdir);

    lua_setfield(
        state,
        -2,
        "mkdir");

    lua_setglobal(
        state,
        "threee");
}

} // namespace

ScriptResult LuaRuntime::ExecuteFile(
    const std::filesystem::path& path,
    const std::string& entryFunction,
    const ScriptContext& context) const {

    ScriptResult result;

    if (path.empty()) {
        result.message =
            "Lua script path is empty.";

        return result;
    }

    std::error_code ec;

    if (!std::filesystem::exists(
            path,
            ec)) {

        result.message =
            "Lua script not found: " +
            path.string();

        return result;
    }

    lua_State* state =
        luaL_newstate();

    if (!state) {
        result.message =
            "Failed to create Lua state.";

        return result;
    }

    luaL_openlibs(state);

    std::string log;

    InstallThreeETable(
        state,
        log);

    PushContext(
        state,
        context);

    lua_setglobal(
        state,
        "Context");

    const std::string scriptPath =
        path.string();

    if (luaL_loadfile(
            state,
            scriptPath.c_str())
        != LUA_OK) {

        result.message =
            "Lua load error: " +
            LuaError(state);

        lua_close(state);
        return result;
    }

    if (lua_pcall(
            state,
            0,
            0,
            0)
        != LUA_OK) {

        result.message =
            "Lua execution error: " +
            LuaError(state);

        lua_close(state);
        return result;
    }

    const std::string functionName =
        entryFunction.empty()
            ? "main"
            : entryFunction;

    lua_getglobal(
        state,
        functionName.c_str());

    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);

        result.success = true;

        result.message =
            log.empty()
                ? "Lua script loaded successfully: " +
                    path.filename().string()
                : log;

        lua_close(state);
        return result;
    }

    if (!lua_isfunction(state, -1)) {
        result.message =
            "Lua entry is not a function: " +
            functionName;

        lua_close(state);
        return result;
    }

    PushContext(
        state,
        context);

    if (lua_pcall(
            state,
            1,
            1,
            0)
        != LUA_OK) {

        result.message =
            "Lua function error: " +
            LuaError(state);

        lua_close(state);
        return result;
    }

    std::string returned;

    if (lua_isstring(state, -1)) {
        size_t length = 0;

        const char* text =
            lua_tolstring(
                state,
                -1,
                &length);

        if (text) {
            returned.assign(
                text,
                length);
        }
    }
    else if (lua_isboolean(state, -1)) {
        if (!lua_toboolean(state, -1)) {
            result.message =
                log.empty()
                    ? "Lua function returned false."
                    : log;

            lua_close(state);
            return result;
        }
    }

    lua_pop(state, 1);

    result.success = true;

    if (!returned.empty()) {
        result.message = returned;
    }
    else if (!log.empty()) {
        result.message = log;
    }
    else {
        result.message =
            "Lua script completed: " +
            path.filename().string();
    }

    lua_close(state);
    return result;
}

} // namespace threee::scripting