print("Hello world!")

World.onPlayerAdded:listen(function(plr)
    print(plr.type .. " " .. plr.name .. " joined from LUA!!")
    plr.onChat:listen(function(msg)
        print(plr.name .. " said : " .. msg)

        if msg == "kickme" then
            plr:kick()
        end
    end)
end)

wait(2)
print("Hello world ~2 seconds later! running protcol version " .. World.version)