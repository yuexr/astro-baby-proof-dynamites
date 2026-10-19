local path = debug.getinfo(1, "S").source:gsub("^@", "")
local directory = path:match("^(.*[/\\])")
if not directory then error("[Baby-Proof Explosions] Can't find the Scripts folder") end
local configPath = directory .. "../config.ini"
local dllPath = directory .. "BabyProofDynamitesNative.dll"
local initialize, reason = package.loadlib(dllPath, "luaopen_babyproofdynamites")
if not initialize then error("[Baby-Proof Explosions] Couldn't load the DLL: " .. tostring(reason)) end
local refresh, refreshReason = package.loadlib(dllPath, "luaopen_babyproofdynamites_refresh")
if not refresh then error("[Baby-Proof Explosions] Couldn't load the config refresh function: " .. tostring(refreshReason)) end
local turnOn, onReason = package.loadlib(dllPath, "luaopen_babyproofdynamites_on")
if not turnOn then error("[Baby-Proof Explosions] Couldn't load the ON function: " .. tostring(onReason)) end
local turnOff, offReason = package.loadlib(dllPath, "luaopen_babyproofdynamites_off")
if not turnOff then error("[Baby-Proof Explosions] Couldn't load the OFF function: " .. tostring(offReason)) end

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
    return { active = true, enabled = enabled == "ON", vehiclesProtected = vehicles == "OFF", text = text }
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
            if not (inSection and key and (key:lower() == keyName:lower() or key:lower() == "allowattachedvehicledestruction")) then
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
    "babyproof vehicle on",
    "babyproof vehicle off",
    "vehicle aliases: vehicles, v (e.g. babyproof v on)",
    "default: ON (safe bombs); on/off lasts until the game restarts"
}, "\n")

local function statusMessage(state)
    if not state.active then
        return "Babyproof: INACTIVE\nEquipment: NOT PROTECTED BY THIS MOD\nVehicles: NOT PROTECTED BY THIS MOD\n" ..
            state.text:gsub("^INACTIVE:%s*", "")
    end
    if not state.enabled then
        return "Babyproof: OFF (vanilla explosions)\nEquipment: NOT PROTECTED\nVehicles: NOT PROTECTED"
    end
    local vehicles = state.vehiclesProtected and "PROTECTED" or "BREAKABLE (direct attachment required)"
    return "Babyproof: ON (tiny explosions)\nEquipment: PROTECTED\nVehicles: " .. vehicles
end

local function reply(output, message)
    local ok, state = pcall(readStatus)
    local summary = ok and statusMessage(state) or
        ("Babyproof: STATUS UNAVAILABLE\nProtection could not be confirmed.\n" .. tostring(state))
    output:Log((message and (message .. "\n\n") or "") .. summary .. "\n")
    return true
end

initialize()
local startupOK, startup = pcall(readStatus)
if startupOK then
    print("[Baby-Proof Explosions] " .. statusMessage(startup) .. "\n")
else
    print("[Baby-Proof Explosions] Check the status file to see if it loaded.\n")
end

RegisterConsoleCommandHandler("babyproof", function(_, parameters, output)
    local group = parameters[1] and parameters[1]:lower()
    local action = parameters[2] and parameters[2]:lower()
    if #parameters == 0 then return reply(output, help) end
    local mainCommand = #parameters == 1 and
        (group == "status" or group == "on" or group == "off")
    local vehicleCommand = #parameters == 2 and (group == "vehicle" or group == "vehicles" or group == "v") and
        (action == "on" or action == "off")
    if not mainCommand and not vehicleCommand then
        return reply(output, "Unknown command.\n" .. help)
    end
    if mainCommand and group == "status" then return reply(output) end
    local ok, message = pcall(function()
        local state = readStatus()
        if not state.active then return "Command not applied." end
        if mainCommand then
            if group == "on" then
                saveSetting("ProtectVehicles", true)
                refresh()
                turnOn()
            else
                turnOff()
            end
            state = readStatus()
            assert(state.active and state.enabled == (group == "on"), "Setting didn't update. Check the status file.")
            if group == "on" then
                assert(state.vehiclesProtected, "Vehicle protection didn't update. Check the status file.")
                return "Vehicle protection saved to config.ini."
            end
            return "This change lasts until the game restarts."
        end
        local protect = action == "on"
        local switchedOn = protect and not state.enabled
        saveSetting("ProtectVehicles", protect)
        refresh()
        if switchedOn then turnOn() end
        state = readStatus()
        assert(state.active and state.vehiclesProtected == protect, "Setting didn't update. Check the status file.")
        if protect then assert(state.enabled, "Babyproof didn't turn ON. Check the status file.") end
        return (switchedOn and "Babyproof turned ON. " or "") .. "Vehicle protection saved to config.ini."
    end)
    if not ok then
        message = "Command failed: " .. tostring(message)
        print("[Baby-Proof Explosions] " .. message .. "\n")
    end
    return reply(output, message)
end)
