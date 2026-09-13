function main(ctx)
    local gameRoot = ctx.GameRoot or ""
    local assets = ctx.ExportedAssets or ""

    if gameRoot == "" then
        error("GameRoot is not available in the active project context.")
    end

    if assets == "" then
        error("ExportedAssets is not available in the active project context.")
    end

    if not threee.exists(gameRoot) then
        error("GameRoot does not exist: " .. gameRoot)
    end

    if not threee.exists(assets) then
        error("ExportedAssets does not exist: " .. assets)
    end

    threee.log("Jackie Lua context is valid.")
    threee.log("GameRoot: " .. gameRoot)
    threee.log("ExportedAssets: " .. assets)

    return "Lua OK - " .. (ctx.ActiveProject or "Jackie project")
end