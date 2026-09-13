function main(ctx)
    threee.log("3E Lua template script")
    threee.log("Active project: " .. (ctx.ActiveProject or "<none>"))

    return "Lua template executed."
end