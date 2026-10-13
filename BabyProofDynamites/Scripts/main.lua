local path = debug.getinfo(1, "S").source:gsub("^@", "")
local directory = path:match("^(.*[/\\])")
if not directory then error("[Baby-Proof Dynamites] Cannot locate Scripts directory") end
local configPath = directory .. "../config.ini"
local dllPath = directory .. "BabyProofDynamitesNative.dll"
local initialize, reason = package.loadlib(dllPath, "luaopen_babyproofdynamites")
if not initialize then error("[Baby-Proof Dynamites] Native helper failed to load: " .. tostring(reason)) end
local refresh, refreshReason = package.loadlib(dllPath, "luaopen_babyproofdynamites_refresh")
if not refresh then error("[Baby-Proof Dynamites] Config refresh failed to load: " .. tostring(refreshReason)) end

local function readStatus()
    local file, err = io.open(directory .. "BabyProofDynamites-status.txt", "r")
    if not file then error("Status file unavailable: " .. tostring(err)) end
    local text = file:read("*a"); file:close()
    if not text:match("^ACTIVE:") then error(text) end
    if text:find("Attached vehicle destruction ON.", 1, true) then return true, text end
    if text:find("Attached vehicle destruction OFF.", 1, true) then return false, text end
    error("Unrecognized native status: " .. text)
end

local function saveSetting(enabled)
    local file, err = io.open(configPath, "r")
    if not file then error("Could not read config.ini: " .. tostring(err)) end
    local original = file:read("*a"); file:close()
    local text = original:gsub("\r\n", "\n")
    local lines, inSection, written = {}, false, false
    for line in (text .. "\n"):gmatch("(.-)\n") do
        local section = line:match("^%s*%[([^%]]+)%]%s*$")
        if section then
            inSection = section:lower() == "explosions"
            lines[#lines + 1] = line
            if inSection and not written then
                lines[#lines + 1] = "AllowAttachedVehicleDestruction=" .. tostring(enabled)
                written = true
            end
        else
            local key = line:match("^%s*([%w_]+)%s*=")
            if not (inSection and key and key:lower() == "allowattachedvehicledestruction") then
                lines[#lines + 1] = line
            end
        end
    end
    if not written then
        lines[#lines + 1] = "[Explosions]"
        lines[#lines + 1] = "AllowAttachedVehicleDestruction=" .. tostring(enabled)
    end
    local updated = table.concat(lines, "\n"):gsub("\n*$", "\n")
    local output = assert(io.open(configPath, "w"))
    local ok, writeError = output:write(updated)
    local closed, closeError = output:close()
    if not ok or not closed then error("Could not save config.ini: " .. tostring(writeError or closeError)) end
end

initialize()
local startupOK, _, startupText = pcall(readStatus)
if startupOK then
    print("[Baby-Proof Dynamites] " .. startupText .. "\n")
else
    print("[Baby-Proof Dynamites] Activation unconfirmed; check the status file.\n")
end

RegisterConsoleCommandHandler("babyproof", function(_, parameters, output)
    local group = parameters[1] and parameters[1]:lower()
    local action = parameters[2] and parameters[2]:lower()
    if #parameters ~= 2 or (group ~= "vehicles" and group ~= "v") or
       (action ~= "on" and action ~= "off" and action ~= "status") then
        output:Log("Usage: babyproof vehicles on|off|status (alias: babyproof v on|off|status).\n")
        return true
    end
    local ok, message = pcall(function()
        local enabled = readStatus()
        if action ~= "status" then
            local requested = action == "on"
            saveSetting(requested)
            refresh()
            enabled = readStatus()
            assert(enabled == requested, "Native setting did not update; check the status file.")
        end
        return "Attached vehicle destruction: " .. (enabled and "ON" or "OFF") ..
            (action == "status" and "." or ". Applied now and saved to config.ini.")
    end)
    output:Log(tostring(message) .. "\n")
    if not ok then print("[Baby-Proof Dynamites] " .. tostring(message) .. "\n") end
    return true
end)