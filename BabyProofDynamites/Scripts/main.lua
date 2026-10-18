local path = debug.getinfo(1, "S").source:gsub("^@", "")
local directory = path:match("^(.*[/\\])")
if not directory then error("[Baby-Proof Dynamites] Can't find the Scripts folder") end
local configPath = directory .. "../config.ini"
local dllPath = directory .. "BabyProofDynamitesNative.dll"
local initialize, reason = package.loadlib(dllPath, "luaopen_babyproofdynamites")
if not initialize then error("[Baby-Proof Dynamites] Couldn't load the DLL: " .. tostring(reason)) end
local refresh, refreshReason = package.loadlib(dllPath, "luaopen_babyproofdynamites_refresh")
if not refresh then error("[Baby-Proof Dynamites] Couldn't load the config refresh function: " .. tostring(refreshReason)) end

local function readStatus()
    local file, err = io.open(directory .. "BabyProofDynamites-status.txt", "r")
    if not file then error("Couldn't read the status file: " .. tostring(err)) end
    local text = file:read("*a"); file:close()
    if not text:match("^ACTIVE:") then
        return { active = false, text = text }
    end
    local enabled = text:match("Safe explosions (%u+)%.")
    local vehicles = text:match("Attached vehicle destruction (%u+)%.")
    if (enabled ~= "ON" and enabled ~= "OFF") or (vehicles ~= "ON" and vehicles ~= "OFF") then
        error("Unknown DLL status: " .. text)
    end
    return { active = true, enabled = enabled == "ON", vehicles = vehicles == "ON", text = text }
end

local function saveSetting(keyName, enabled)
    local file, err = io.open(configPath, "r")
    if not file then error("Couldn't read config.ini: " .. tostring(err)) end
    local original = file:read("*a"); file:close()
    local text = original:gsub("\r\n", "\n")
    local lines, inSection, written = {}, false, false
    for line in (text .. "\n"):gmatch("(.-)\n") do
        local section = line:match("^%s*%[([^%]]+)%]%s*$")
        if section then
            inSection = section:lower() == "explosions"
            lines[#lines + 1] = line
            if inSection and not written then
                lines[#lines + 1] = keyName .. "=" .. tostring(enabled)
                written = true
            end
        else
            local key = line:match("^%s*([%w_]+)%s*=")
            if not (inSection and key and key:lower() == keyName:lower()) then
                lines[#lines + 1] = line
            end
        end
    end
    if not written then
        lines[#lines + 1] = "[Explosions]"
        lines[#lines + 1] = keyName .. "=" .. tostring(enabled)
    end
    local updated = table.concat(lines, "\n"):gsub("\n*$", "\n")
    local output = assert(io.open(configPath, "w"))
    local ok, writeError = output:write(updated)
    local closed, closeError = output:close()
    if not ok or not closed then error("Couldn't save config.ini: " .. tostring(writeError or closeError)) end
end

local help = table.concat({
    "babyproof (show commands)",
    "babyproof status",
    "babyproof on",
    "babyproof off",
    "babyproof vehicles on",
    "babyproof vehicles off",
    "babyproof vehicles status",
    "vehicles alias: v (e.g. babyproof v on)",
    "default: ON (safe bombs)"
}, "\n")

local function statusMessage(state)
    if not state.active then return "Babyproof: INACTIVE\n" .. state.text:gsub("^INACTIVE:%s*", "") end
    local mode = state.enabled and "ON (safe bombs)" or "OFF (vanilla explosions)"
    local vehicles = state.vehicles and "ON" or "OFF"
    if not state.enabled then vehicles = vehicles .. " (saved; applies when Babyproof is ON)" end
    return "Mod: ACTIVE\nBabyproof: " .. mode .. "\nVehicle destruction: " .. vehicles
end

initialize()
local startupOK, startup = pcall(readStatus)
if startupOK then
    print("[Baby-Proof Dynamites] " .. statusMessage(startup) .. "\n")
else
    print("[Baby-Proof Dynamites] Check the status file to see if it loaded.\n")
end

RegisterConsoleCommandHandler("babyproof", function(_, parameters, output)
    local group = parameters[1] and parameters[1]:lower()
    local action = parameters[2] and parameters[2]:lower()
    if #parameters == 0 then output:Log(help .. "\n"); return true end
    local mainCommand = #parameters == 1 and
        (group == "status" or group == "on" or group == "off")
    local vehicleCommand = #parameters == 2 and (group == "vehicles" or group == "v") and
        (action == "on" or action == "off" or action == "status")
    if not mainCommand and not vehicleCommand then
        output:Log("Unknown command.\n" .. help .. "\n")
        return true
    end
    local ok, message = pcall(function()
        local state = readStatus()
        if mainCommand and group == "status" then return statusMessage(state) end
        if not state.active then return statusMessage(state) end
        if mainCommand then
            local requested = group == "on"
            saveSetting("Enabled", requested)
            refresh()
            state = readStatus()
            assert(state.active and state.enabled == requested, "Setting didn't update. Check the status file.")
            return "Babyproof: " .. (state.enabled and "ON (safe bombs)." or "OFF (vanilla explosions).") ..
                " Saved to config.ini."
        end
        if action ~= "status" then
            local requested = action == "on"
            saveSetting("AllowAttachedVehicleDestruction", requested)
            refresh()
            state = readStatus()
            assert(state.active and state.vehicles == requested, "Setting didn't update. Check the status file.")
        end
        local result = "Vehicle destruction: " .. (state.vehicles and "ON" or "OFF") .. "."
        if not state.enabled then result = result .. " Applies when Babyproof is ON." end
        if action ~= "status" then result = result .. " Saved to config.ini." end
        return result
    end)
    output:Log(tostring(message) .. "\n")
    if not ok then print("[Baby-Proof Dynamites] " .. tostring(message) .. "\n") end
    return true
end)
