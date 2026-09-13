local lua_root = path.getdirectory(_SCRIPT)

project "lua"
    kind "StaticLib"
    language "C"
    staticruntime "on"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("%{wks.location}/obj/" .. outputdir .. "/%{prj.name}")

    files {
        lua_root .. "/src/*.c",
        lua_root .. "/src/*.h",
    }

    removefiles {
        lua_root .. "/src/lua.c",
        lua_root .. "/src/luac.c",
        lua_root .. "/src/onelua.c",
    }

    includedirs {
        lua_root .. "/src",
    }

    filter "system:windows"
        defines { "LUA_USE_WINDOWS" }

    filter "system:linux"
        defines { "LUA_USE_LINUX" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release*"
        runtime "Release"
        optimize "on"

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"

    filter "configurations:Headless"
        runtime "Release"
        optimize "on"

    filter {}