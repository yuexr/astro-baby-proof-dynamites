local path = debug.getinfo(1, "S").source:gsub("^@", "")
local directory = path:match("^(.*[/\\])")
if not directory then error("[Baby-Proof Dynamites] Cannot locate Scripts directory") end
local initialize, reason = package.loadlib(directory .. "BabyProofDynamitesNative.dll", "luaopen_babyproofdynamites")
if not initialize then error("[Baby-Proof Dynamites] Native helper failed to load: " .. tostring(reason)) end
initialize()
local status = io.open(directory .. "BabyProofDynamites-status.txt", "r")
if status then
    print("[Baby-Proof Dynamites] " .. status:read("*a") .. "\n")
    status:close()
else
    print("[Baby-Proof Dynamites] Status file unavailable; activation is unconfirmed.\n")
end
