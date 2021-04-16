print("Hello world!")

World.onPlayerAdded:listen(function(plr)
    print(plr.type .. " " .. plr.name .. " joined from LUA!!")
end)

wait(2)
print("Hello world ~2 seconds later! running protcol version " .. World.version)