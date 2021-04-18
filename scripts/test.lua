print("Hello wtf!")

function onJoin(plr)
    print(plr.type .. " " .. plr.name .. " joined from LUA!!")
    plr.onChat:listen(function(msg)
        print(plr.name .. " said : \'" .. msg .. "\'")

        if msg == "kickme" then
            plr:kick()
        elseif msg == "hi" then
            print("hello " .. plr.name)
        end
    end)
end

World.onPlayerAdded:listen(onJoin)

for i, plr in ipairs(World.players) do
    onJoin(plr)
end

wait(2)
print("Hello world ~2 seconds later! running protcol version " .. World.version)