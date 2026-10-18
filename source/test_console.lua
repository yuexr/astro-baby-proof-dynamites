local script = assert(arg[1])
local scripts = assert(script:match('^(.*)[/\\]main.lua$'))
local config = scripts .. '/../config.ini'
local status = scripts .. '/BabyProofDynamites-status.txt'
local handler, enabled, vehicles, active = nil, true, false, true
local messages = {}
local function write(path, text) local f=assert(io.open(path,'w')); assert(f:write(text)); assert(f:close()) end
local function read(path) local f=assert(io.open(path,'r'));local s=f:read('*a');f:close();return s end
local function updateStatus()
    local text = active and ('ACTIVE: Safe explosions '..(enabled and 'ON.' or 'OFF.')..
        ' Attached vehicle destruction '..(vehicles and 'ON.' or 'OFF.')) or 'INACTIVE: unsupported build'
    write(status, text)
end
local function loadSettings()
    local text=read(config):match('%[Explosions%]([^%[]*)') or ''
    enabled=not text:find('Enabled=false',1,true)
    vehicles=text:find('AllowAttachedVehicleDestruction=true',1,true)~=nil
    updateStatus()
end
write(config,'; keep this comment\n[Explosions]\nEnabled=true\nAllowAttachedVehicleDestruction=false\nOtherSetting=123\n[Other]\nEnabled=leave-this-alone\nKeep=true\n')
package.loadlib=function(_,name)
    assert(name=='luaopen_babyproofdynamites' or name=='luaopen_babyproofdynamites_refresh')
    return loadSettings
end
RegisterConsoleCommandHandler=function(name,fn) assert(name=='babyproof');handler=fn end
local output={Log=function(_,text) messages[#messages+1]=text end}
local function command(...)
    assert(handler('',{...},output))
    return messages[#messages]
end
dofile(script)
assert(enabled and not vehicles)
local before=read(config)
local help=command()
assert(not help:find('babyproof toggle',1,true) and help:find('babyproof vehicles status',1,true))
assert(read(config)==before)
assert(command('status')=='Mod: ACTIVE\nBabyproof: ON (safe bombs)\nVehicle destruction: OFF\n')
assert(command('v','on')=='Vehicle destruction: ON. Saved to config.ini.\n')
assert(vehicles and enabled)
assert(command('off')=='Babyproof: OFF (vanilla explosions). Saved to config.ini.\n')
assert(not enabled and vehicles)
assert(command('status'):find('ON (saved; applies when Babyproof is ON)',1,true))
assert(command('vehicles','off'):find('Applies when Babyproof is ON.',1,true))
assert(not enabled and not vehicles)
assert(command('on')=='Babyproof: ON (safe bombs). Saved to config.ini.\n')
assert(enabled and not vehicles)
assert(command('OFF'):find('OFF (vanilla explosions)',1,true))
assert(not enabled)
assert(command('ON'):find('ON (safe bombs)',1,true))
assert(enabled)
command('V','ON');assert(vehicles)
assert(command('vehicles','status')=='Vehicle destruction: ON.\n')
command('off');assert(not enabled and vehicles)
loadSettings();assert(not enabled and vehicles)
command('on');assert(enabled and vehicles)
local saved=read(config)
assert(saved:find('; keep this comment',1,true) and saved:find('OtherSetting=123',1,true))
assert(saved:find('[Other]\nEnabled=leave-this-alone\nKeep=true',1,true))
for _,params in ipairs({{'bogus'},{'toggle'},{'toggle','on'},{'v','bogus'}}) do
    assert(command(table.unpack(params)):find('Unknown command.',1,true))
    assert(read(config)==saved and enabled and vehicles)
end
active=false;updateStatus()
assert(command('status')=='Babyproof: INACTIVE\nunsupported build\n')
assert(command()==help)
command('off');command('v','off')
assert(read(config)==saved and enabled and vehicles)
print('PASS: help/status/main on/off; removed toggle rejected; vehicle aliases and independent saved state; restart persistence; unrelated config preserved; unknown and inactive commands handled.')
